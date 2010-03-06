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
        C4Node *expr = (C4Node *) lc_ptr(lc);
        AstVarExpr *var;

        ASSERT(expr->kind == AST_VAR_EXPR);
        var = (AstVarExpr *) expr;

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
        C4Node *n = (C4Node *) lc_ptr(lc);
        ExprVar *expr_var;

        ASSERT(n->kind == EXPR_VAR);
        expr_var = (ExprVar *) n;

        if (strcmp(expr_var->name, var_name) == 0)
            return i;

        i++;
    }

    return -1;
}

static int
get_var_index(const char *var_name, AstJoinClause *delta_tbl,
              AstJoinClause *outer_rel, PlannerState *state, bool *is_outer)
{
    int var_idx;

    *is_outer = false;

    /*
     * If we haven't done projection yet, we need to look for variables in the
     * delta table's join clause. Otherwise use the current projection list.
     */
    if (state->current_plist == NULL)
        var_idx = get_var_index_from_join(var_name, delta_tbl);
    else
        var_idx = get_var_index_from_plist(var_name, state->current_plist);

    if (var_idx == -1 && outer_rel != NULL)
    {
        var_idx = get_var_index_from_join(var_name, outer_rel);
        if (var_idx != -1)
            *is_outer = true;
    }

    return var_idx;
}

/*
 * Convert the AST representation of an expression tree into the Eval
 * ("runtime") representation.
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
        /*
         * XXX: An AggExpr will only appear in the projection list for an
         * AggPlan. We actually want to IGNORE the agg expr itself, and only
         * produce a runtime expr for the agg expr's operand. This is because
         * the AggPlan will apply the agg function(s) itself: projection should
         * just prepare the input tuple to the agg's transition function.
         */
        case AST_AGG_EXPR:
            {
                AstAggExpr *ast_agg = (AstAggExpr *) ast_expr;
                result = make_eval_expr(ast_agg->expr, outer_rel,
                                        chain_plan, state);
            }
            break;

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
                bool is_outer;
                DataType expr_type;

                var_idx = get_var_index(ast_var->name, chain_plan->delta_tbl,
                                        outer_rel, state, &is_outer);
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
    AstJoinClause *outer_rel;
    int var_idx;
    bool is_outer;

    /*
     * Note that if the outer relation is anti-scanned, we can't project any
     * variables from it: projection is only applied to anti-scan outputs when
     * there isn't a matching tuple in the outer relation.
     */
    outer_rel = cxt->outer_rel;
    if (outer_rel && outer_rel->not)
        outer_rel = NULL;

    var_idx = get_var_index(var->name, cxt->delta_tbl, outer_rel,
                            cxt->state, &is_outer);

    if (var_idx == -1)
    {
        List *eq_list;
        ListCell *lc;

        eq_list = get_eq_list(var->name, true, cxt->state->var_eq_tbl,
                              cxt->state->tmp_pool);

        foreach (lc, eq_list)
        {
            char *v = (char *) lc_ptr(lc);

            var_idx = get_var_index(v, cxt->delta_tbl, outer_rel,
                                    cxt->state, &is_outer);
            if (var_idx != -1)
                break;
        }
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
make_tbl_ref_proj_list(AstTableRef *tbl_ref, AstJoinClause *outer_rel,
                       OpChainPlan *chain_plan, PlannerState *state)
{
    List *proj_list;
    ListCell *lc;

    proj_list = list_make(state->plan_pool);

    foreach (lc, tbl_ref->cols)
    {
        C4Node *ast_expr = (C4Node *) lc_ptr(lc);
        ExprNode *expr;

        expr = make_eval_expr(ast_expr, outer_rel, chain_plan, state);
        list_append(proj_list, expr);
    }

    return proj_list;
}

static List *
make_dummy_proj_list(OpChainPlan *chain_plan, PlannerState *state)
{
    List *result;
    ListCell *lc;
    int i;

    if (state->current_plist == NULL)
        return make_tbl_ref_proj_list(chain_plan->delta_tbl->ref,
                                      NULL, chain_plan, state);

    result = list_make(state->plan_pool);
    i = 0;
    foreach (lc, state->current_plist)
    {
        ExprNode *n = (ExprNode *) lc_ptr(lc);
        ExprVar *v;

        v = make_expr_var(n->type, i, false, "_",
                          state->plan_pool);
        list_append(result, v);
        i++;
    }

    return result;
}

static List *
make_proj_list(PlanNode *plan, ListCell *chain_rest, AstJoinClause *outer_rel,
               OpChainPlan *chain_plan, PlannerState *state)
{
    ProjListContext cxt;
    ListCell *lc;

    /* PLAN_INSERT and PLAN_AGG don't do projection */
    if (plan->node.kind == PLAN_INSERT || plan->node.kind == PLAN_AGG)
    {
        ASSERT(chain_rest == NULL);
        return make_dummy_proj_list(chain_plan, state);
    }

    /*
     * XXX: We currently assume that PLAN_PROJECT is followed by either
     * PLAN_INSERT or PLAN_AGG; in either case, we want to do projection based
     * on the rule's head clause.
     */
    if (plan->node.kind == PLAN_PROJECT)
        return make_tbl_ref_proj_list(chain_plan->head, outer_rel,
                                      chain_plan, state);

    /*
     * We also handle PLAN_FILTER specially: the filter operator never does
     * projection because it does not modify its input, so there is little to be
     * gained by only projecting out the attributes we need for the rest of the
     * operator chain. Hence, we just cookup a projection list that is
     * equivalent to the filter's input schema.
     */
    if (plan->node.kind == PLAN_FILTER)
        return make_tbl_ref_proj_list(chain_plan->delta_tbl->ref,
                                      outer_rel, chain_plan, state);

    /*
     * Check if this is a PLAN_SCAN followed immediately by a PLAN_AGG or
     * PLAN_INSERT. When this is the case, we can skip projection in the
     * agg/insert op, and instead do any necessary projection when producing the
     * output of the scan. We can't always apply this optimization, since an
     * agg/insert might be preceded by a filter or might be the first op in a
     * chain.
     */
    if (plan->node.kind == PLAN_SCAN)
    {
        PlanNode *next;

        ASSERT(chain_rest != NULL);
        next = lc_ptr(chain_rest);
        if (next->node.kind == PLAN_AGG || next->node.kind == PLAN_INSERT)
        {
            next->skip_proj = true;
            return make_tbl_ref_proj_list(chain_plan->head, outer_rel,
                                          chain_plan, state);
        }
    }

    cxt.wip_plist = list_make(state->plan_pool);
    cxt.outer_rel = outer_rel;
    cxt.delta_tbl = chain_plan->delta_tbl;
    cxt.state = state;

    forrest (lc, chain_rest)
    {
        PlanNode *sub_plan = (PlanNode *) lc_ptr(lc);
        ListCell *lc2;

        /*
         * For each expression subtree in every downstream element in the op
         * chain, add to the WIP projection list all the variable names that (a)
         * are referenced (b) are defined by either the current plist or by the
         * scan relation (c) are not already in WIP projection list.
         */
        foreach (lc2, sub_plan->quals)
        {
            C4Node *qual = (C4Node *) lc_ptr(lc2);

            expr_tree_var_walker(qual, make_proj_list_walker, &cxt);
        }

        if (sub_plan->node.kind == PLAN_INSERT)
        {
            InsertPlan *iplan = (InsertPlan *) sub_plan;

            expr_tree_var_walker((C4Node *) iplan->head,
                                 make_proj_list_walker, &cxt);
        }

        if (sub_plan->node.kind == PLAN_AGG)
        {
            AggPlan *aplan = (AggPlan *) sub_plan;

            expr_tree_var_walker((C4Node *) aplan->head,
                                 make_proj_list_walker, &cxt);
        }
    }

    return cxt.wip_plist;
}

static void
add_qual_expr(AstQualifier *qual, PlanNode *plan, AstJoinClause *outer_rel,
              OpChainPlan *chain_plan, PlannerState *state)
{
    ExprNode *expr;
    AstVarExpr *lhs_var;
    AstVarExpr *rhs_var;

    expr = make_eval_expr(qual->expr, outer_rel, chain_plan, state);
    list_append(plan->qual_exprs, expr);

    /* Update var equality table */
    if (is_simple_equality(qual, &lhs_var, &rhs_var))
    {
        add_var_eq(lhs_var->name, rhs_var->name,
                   state->var_eq_tbl, state->tmp_pool);
        add_var_eq(rhs_var->name, lhs_var->name,
                   state->var_eq_tbl, state->tmp_pool);
    }
}

static void
fix_op_exprs(PlanNode *plan, ListCell *chain_rest,
             OpChainPlan *chain_plan, PlannerState *state)
{
    AstJoinClause *outer_rel = NULL;
    ListCell *lc;
    List *new_plist;

    if (plan->node.kind == PLAN_AGG)
    {
        AggPlan *agg_plan = (AggPlan *) plan;

        if (agg_plan->planned)
            return;

        agg_plan->planned = true;
    }

    /* Lookup the outer (scan) relation, if any */
    if (plan->node.kind == PLAN_SCAN)
    {
        ScanPlan *scan_plan = (ScanPlan *) plan;
        outer_rel = scan_plan->scan_rel;
    }

    ASSERT(plan->qual_exprs == NULL);
    plan->qual_exprs = list_make(state->plan_pool);

    foreach (lc, plan->quals)
    {
        AstQualifier *qual = (AstQualifier *) lc_ptr(lc);
        add_qual_expr(qual, plan, outer_rel, chain_plan, state);
    }

    new_plist = make_proj_list(plan, chain_rest, outer_rel, chain_plan, state);
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
    apr_hash_clear(state->var_eq_tbl);

    foreach (lc, chain_plan->chain)
    {
        PlanNode *node = (PlanNode *) lc_ptr(lc);

        fix_op_exprs(node, lc->next, chain_plan, state);
    }
}
