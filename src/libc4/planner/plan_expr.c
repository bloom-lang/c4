#include "c4-internal.h"
#include "nodes/makefuncs.h"
#include "parser/analyze.h"
#include "parser/walker.h"
#include "planner/planner-internal.h"
#include "types/datum.h"

static Datum
eval_const_expr(AstConstExpr *ast_const, PlannerState *state)
{
    DataType const_type;
    Datum result;

    const_type = expr_get_type((C4Node *) ast_const);
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
        ExprVar *expr_var = (ExprVar *) lc_ptr(lc);

        if (strcmp(expr_var->name, var_name) == 0)
            return i;

        i++;
    }

    return -1;
}

/*
 * Convert the AST representation of an expression tree into the Eval
 * representation.
 *
 * XXX: Terribly ugly. Should use a uniform interface for finding the index
 * associated with a given variable name.
 */
static ExprNode *
make_eval_expr(C4Node *ast_expr, AstJoinClause *outer_rel,
               OpChainPlan *chain_plan, PlannerState *state)
{
    ExprNode *result;

    switch (ast_expr->kind)
    {
        case AST_CONST_EXPR:
            {
                AstConstExpr *ast_const = (AstConstExpr *) ast_expr;
                Datum const_val;
                ExprConst *expr_const;

                const_val = eval_const_expr(ast_const, state);
                expr_const = apr_pcalloc(state->plan_pool, sizeof(*expr_const));
                expr_const->expr.node.kind = EXPR_CONST;
                expr_const->expr.type = expr_get_type((C4Node *) ast_const);
                expr_const->value = const_val;
                result = (ExprNode *) expr_const;
            }
            break;

        case AST_OP_EXPR:
            {
                AstOpExpr *ast_op = (AstOpExpr *) ast_expr;
                ExprOp *expr_op;

                expr_op = apr_pcalloc(state->plan_pool, sizeof(*expr_op));
                expr_op->expr.node.kind = EXPR_OP;
                expr_op->expr.type = expr_get_type((C4Node *) ast_op);
                expr_op->op_kind = ast_op->op_kind;
                expr_op->lhs = make_eval_expr(ast_op->lhs, outer_rel, chain_plan, state);
                if (ast_op->rhs)
                    expr_op->rhs = make_eval_expr(ast_op->rhs, outer_rel, chain_plan, state);
                result = (ExprNode *) expr_op;
            }
            break;

        case AST_VAR_EXPR:
            {
                AstVarExpr *ast_var = (AstVarExpr *) ast_expr;
                int var_idx;
                bool is_outer = false;
                DataType expr_type;

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

                expr_type = expr_get_type((C4Node *) ast_var);
                result = (ExprNode *) make_expr_var(expr_type,
                                                    var_idx, is_outer,
                                                    ast_var->name,
                                                    state->plan_pool);
            }
            break;

        default:
            ERROR("Unexpected expr node kind: %d", (int) ast_expr->kind);
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
make_proj_list_walker(AstVarExpr *var, void *data)
{
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

            expr_var = make_expr_var(expr_get_type((C4Node *) var),
                                     var_idx, is_outer, var->name,
                                     cxt->state->plan_pool);
            list_append(cxt->wip_plist, expr_var);
        }
    }

    return true;        /* Continue walking the expr tree */
}

static List *
make_tbl_ref_proj_list(AstTableRef *tbl_ref, OpChainPlan *chain_plan,
                       PlannerState *state)
{
    List *proj_list;
    ListCell *lc;

    proj_list = list_make(state->plan_pool);

    foreach (lc, tbl_ref->cols)
    {
        AstColumnRef *cref = (AstColumnRef *) lc_ptr(lc);
        ExprNode *expr;

        expr = make_eval_expr(cref->expr, NULL, chain_plan, state);
        list_append(proj_list, expr);
    }

    return proj_list;
}

static List *
make_insert_proj_list(InsertPlan *iplan, OpChainPlan *chain_plan,
                      PlannerState *state)
{
    return make_tbl_ref_proj_list(iplan->head, chain_plan, state);
}

static List *
make_filter_proj_list(OpChainPlan *chain_plan, PlannerState *state)
{
    return make_tbl_ref_proj_list(chain_plan->delta_tbl->ref,
                                  chain_plan, state);
}

static List *
make_proj_list(PlanNode *plan, ListCell *chain_rest, AstJoinClause *outer_rel,
               OpChainPlan *chain_plan, PlannerState *state)
{
    ProjListContext cxt;
    ListCell *lc;

    /*
     * We need to handle PLAN_INSERT specially: its projection list is based
     * on the rule's head clause, not the expressions that appear in the
     * rest of the chain (chain_rest).
     */
    if (plan->node.kind == PLAN_INSERT)
    {
        ASSERT(chain_rest == NULL);
        return make_insert_proj_list((InsertPlan *) plan,
                                     chain_plan, state);
    }

    /*
     * We also handle PLAN_FILTER specially: we actually skip projection in
     * this case, because the delta filter doesn't modify its input, so
     * there is little to be gained by only projecting out the attributes we
     * need for the rest of the operator chain. Therefore, cookup a
     * projection list that is equivalent to the filter's input schema.
     */
    if (plan->node.kind == PLAN_FILTER)
        return make_filter_proj_list(chain_plan, state);

    cxt.wip_plist = list_make(state->plan_pool);
    cxt.outer_rel = outer_rel;
    cxt.delta_tbl = chain_plan->delta_tbl;
    cxt.state = state;

    forrest (lc, chain_rest)
    {
        PlanNode *plan = (PlanNode *) lc_ptr(lc);
        ListCell *lc2;

        /*
         * For each expression subtree in the plan, add to the WIP
         * projection list all the variable names that (a) are referenced
         * (b) are defined by either the current plist or by the scan
         * relation (c) are not already in WIP projection list.
         */
        foreach (lc2, plan->quals)
        {
            C4Node *qual = (C4Node *) lc_ptr(lc2);

            expr_tree_var_walker(qual, make_proj_list_walker, &cxt);
        }

        if (plan->node.kind == PLAN_INSERT)
        {
            InsertPlan *iplan = (InsertPlan *) plan;

            expr_tree_var_walker((C4Node *) iplan->head,
                                 make_proj_list_walker, &cxt);
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

        expr = make_eval_expr(qual->expr, scan_rel, chain_plan, state);
        list_append(plan->qual_exprs, expr);
    }

    new_plist = make_proj_list(plan, chain_rest, scan_rel, chain_plan, state);
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
