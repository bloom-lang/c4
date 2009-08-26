#include "col-internal.h"
#include "parser/analyze.h"
#include "parser/walker.h"
#include "planner/planner-internal.h"
#include "types/datum.h"

static Datum
eval_const_expr(AstConstExpr *ast_const, PlannerState *state)
{
    DataType const_type;
    Datum result;

    const_type = expr_get_type((ColNode *) ast_const);
    result = datum_from_str(const_type, ast_const->value);
    pool_track_datum(state->plan_pool, result, const_type);
    return result;
}

static int
get_var_index_from_join(const char *var_name, AstJoinClause *join)
{
    ListCell *lc;
    int i;

    i = 0;
    foreach (lc, join->ref->cols)
    {
        AstColumnRef *col = (AstColumnRef *) lc_ptr(lc);
        AstVarExpr *var;

        ASSERT(col->expr->kind == AST_VAR_EXPR);
        var = (AstVarExpr *) col->expr;

        if (strcmp(var->name, var_name) == 0)
            return i;

        i++;
    }

    return -1;
}

static int
get_var_index_from_plist(const char *var_name, List *proj_list)
{
    ListCell *lc;
    int i;

    i = 0;
    foreach (lc, proj_list)
    {
        char *s = (char *) lc_ptr(lc);

        if (strcmp(var_name, s) == 0)
            return i;

        i++;
    }

    return -1;
}

/*
 * XXX: Terribly ugly. Should use a uniform interface for finding the index
 * associated with a given variable name.
 */
static ExprNode *
fix_qual_expr(ColNode *ast_qual, AstJoinClause *outer_rel,
              OpChainPlan *chain_plan, PlannerState *state)
{
    ExprNode *result;

    switch (ast_qual->kind)
    {
        case AST_CONST_EXPR:
            {
                AstConstExpr *ast_const = (AstConstExpr *) ast_qual;
                Datum const_val;
                ExprConst *expr_const;

                const_val = eval_const_expr(ast_const, state);
                expr_const = apr_pcalloc(state->plan_pool, sizeof(*expr_const));
                expr_const->expr.node.kind = EXPR_CONST;
                expr_const->expr.type = expr_get_type((ColNode *) ast_const);
                expr_const->value = const_val;
                result = (ExprNode *) expr_const;
            }
            break;

        case AST_OP_EXPR:
            {
                AstOpExpr *ast_op = (AstOpExpr *) ast_qual;
                ExprOp *expr_op;

                expr_op = apr_pcalloc(state->plan_pool, sizeof(*expr_op));
                expr_op->expr.node.kind = EXPR_OP;
                expr_op->expr.type = expr_get_type((ColNode *) ast_op);
                expr_op->op_kind = ast_op->op_kind;
                expr_op->lhs = fix_qual_expr(ast_op->lhs, outer_rel, chain_plan, state);
                expr_op->rhs = fix_qual_expr(ast_op->rhs, outer_rel, chain_plan, state);
                result = (ExprNode *) expr_op;
            }
            break;

        case AST_VAR_EXPR:
            {
                AstVarExpr *ast_var = (AstVarExpr *) ast_qual;
                int var_idx;
                bool is_outer = false;
                ExprVar *expr_var;

                /*
                 * If we haven't done projection yet, we need to look for
                 * variables in the delta table's join clause. Otherwise use
                 * the current projection list.
                 */
                if (state->current_plist == NULL)
                    var_idx = get_var_index_from_join(ast_var->name, chain_plan->delta_tbl);
                else
                    var_idx = get_var_index_from_plist(ast_var->name, state->current_plist);

                /*
                 * If the variable isn't in the projection list of the input
                 * to this operator, this must be a join and the variable is
                 * the scanned ("outer") relation.
                 */
                if (var_idx == -1 && outer_rel != NULL)
                {
                    is_outer = true;
                    var_idx = get_var_index_from_join(ast_var->name, outer_rel);
                }

                if (var_idx == -1)
                    ERROR("Failed to find variable %s", ast_var->name);

                expr_var = apr_pcalloc(state->plan_pool, sizeof(*expr_var));
                expr_var->expr.node.kind = EXPR_VAR;
                expr_var->expr.type = expr_get_type((ColNode *) ast_var);
                expr_var->attno = var_idx;
                expr_var->is_outer = is_outer;
                result = (ExprNode *) expr_var;
            }
            break;

        default:
            ERROR("Unexpected expr node kind: %d", (int) ast_qual->kind);
    }

    return result;
}

typedef struct ProjListContext
{
    List *wip_plist;
    AstJoinClause *outer_rel;
    AstJoinClause *delta_tbl;
    PlannerState *state;
} ProjListContext;

static bool
make_proj_list_walker(ColNode *n, void *data)
{
    if (n->kind == AST_VAR_EXPR)
    {
        AstVarExpr *var = (AstVarExpr *) n;
        ProjListContext *cxt = (ProjListContext *) data;
        int var_idx;
        bool is_outer = false;

        if (cxt->state->current_plist == NULL)
            var_idx = get_var_index_from_join(var->name, cxt->delta_tbl);
        else
            var_idx = get_var_index_from_plist(var->name, cxt->state->current_plist);

        if (var_idx == -1 && cxt->outer_rel != NULL)
        {
            var_idx = get_var_index_from_join(var->name, cxt->outer_rel);
            is_outer = true;
        }

        if (var_idx != -1)
        {
            /* Don't add to proj list if it already contains this var */
            if (get_var_index_from_plist(var->name, cxt->wip_plist) == -1)
            {
                ExprVar *expr_var;

                expr_var = apr_pcalloc(cxt->state->plan_pool, sizeof(*expr_var));
                expr_var->expr.node.kind = EXPR_VAR;
                expr_var->expr.type = expr_get_type((ColNode *) var);
                expr_var->attno = var_idx;
                expr_var->is_outer = is_outer;
                list_append(cxt->wip_plist, expr_var);
            }
        }
    }

    return true;        /* Continue walking the expr tree */
}

static List *
make_proj_list(ListCell *chain_rest, AstJoinClause *outer_rel,
               AstJoinClause *delta_tbl, PlannerState *state)
{
    ProjListContext cxt;
    ListCell *lc;

    cxt.wip_plist = list_make(state->plan_pool);
    cxt.outer_rel = outer_rel;
    cxt.delta_tbl = delta_tbl;
    cxt.state = state;

    forrest (lc, chain_rest)
    {
        PlanNode *plan = (PlanNode *) lc_ptr(lc);
        ListCell *lc2;

        /*
         * For each expression subtree in the plan, get the list of variable
         * names that (a) are referenced (b) are defined by either the
         * current plist or by the scan relation (c) are not already in
         * proj_list.
         */
        foreach (lc2, plan->quals)
        {
            ColNode *qual = (ColNode *) lc_ptr(lc);

            expr_tree_walker(qual, make_proj_list_walker, &cxt);
        }

        if (plan->node.kind == PLAN_INSERT)
        {
            InsertPlan *iplan = (InsertPlan *) plan;

            expr_tree_walker((ColNode *) iplan->head, make_proj_list_walker, &cxt);
        }
    }

    return cxt.wip_plist;
}

static void
fix_op_exprs(PlanNode *plan, ListCell *chain_rest,
             OpChainPlan *chain_plan, PlannerState *state)
{
    AstJoinClause *scan_rel = NULL;
    ListCell *lc;
    List *new_plist;

    /* Lookup the outer (scan) relation, if any */
    if (plan->node.kind == PLAN_SCAN)
    {
        ScanPlan *scan_plan = (ScanPlan *) plan;
        scan_rel = scan_plan->scan_rel;
    }

    ASSERT(plan->qual_exprs == NULL);
    plan->qual_exprs = list_make(state->plan_pool);

    foreach (lc, plan->quals)
    {
        AstQualifier *qual = (AstQualifier *) lc_ptr(lc);
        ExprNode *expr;

        expr = fix_qual_expr(qual->expr, scan_rel, chain_plan, state);
        list_append(plan->qual_exprs, expr);
    }

    new_plist = make_proj_list(chain_rest, scan_rel,
                               chain_plan->delta_tbl, state);
    ASSERT(plan->proj_list == NULL);
    plan->proj_list = new_plist;
    state->current_plist = new_plist;
}

/*
 * Walk the operator chain from beginning to end. For each operator, (a)
 * compute the minimal projection list that contains all the variables
 * required by subsequent operators in the chain (b) convert Expr from AST
 * representation to Eval representation.
 */
void
fix_chain_exprs(OpChainPlan *chain_plan, PlannerState *state)
{
    ListCell *lc;

    /* Initial proj list is empty => no projection before delta filter */
    state->current_plist = NULL;

    foreach (lc, chain_plan->chain)
    {
        PlanNode *node = (PlanNode *) lc_ptr(lc);

        fix_op_exprs(node, lc->next, chain_plan, state);
    }
}
