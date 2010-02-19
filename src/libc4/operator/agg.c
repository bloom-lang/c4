#include "c4-internal.h"
#include "operator/agg.h"

static void
agg_invoke(Operator *op, Tuple *t)
{
    AggOperator *agg_op = (AggOperator *) op;
    ExprEvalContext *exec_cxt;
    Tuple *proj_tuple;

    exec_cxt = agg_op->op.exec_cxt;
    exec_cxt->inner = t;

    proj_tuple = operator_do_project(op);

    tuple_unpin(proj_tuple, op->proj_schema);
}

static void
agg_destroy(Operator *op)
{
    operator_destroy(op);
}

/*
 * We assume that AggExprs appear as top-level operators (i.e. aggs cannot be
 * nested within expression trees); the analysis phase currently enforces
 * this. Note that this only works with the AST representation of expressions:
 * see make_eval_expr().
 */
static int
count_agg_exprs(AstTableRef *head)
{
    int num_aggs;
    ListCell *lc;

    num_aggs = 0;
    foreach (lc, head->cols)
    {
        AstColumnRef *cref = (AstColumnRef *) lc_ptr(lc);

        if (cref->expr->kind == AST_AGG_EXPR)
            num_aggs++;
    }

    ASSERT(num_aggs > 0);
    return num_aggs;
}

static AggExprInfo **
make_agg_info(int num_aggs, AstTableRef *head, apr_pool_t *pool)
{
    AggExprInfo **result;
    ListCell *lc;
    int aggno;
    int colno;

    result = apr_palloc(pool, num_aggs * sizeof(*result));
    aggno = 0;
    colno = 0;
    foreach (lc, head->cols)
    {
        AstColumnRef *cref = (AstColumnRef *) lc_ptr(lc);
        AstAggExpr *agg_expr;
        AggExprInfo *agg_info;

        if (cref->expr->kind != AST_AGG_EXPR)
        {
            colno++;
            continue;
        }

        agg_expr = (AstAggExpr *) cref->expr;
        agg_info = apr_palloc(pool, sizeof(*agg_info));
        agg_info->colno = colno;
        agg_info->agg_kind = agg_expr->agg_kind;
        result[aggno] = agg_info;
        aggno++;
        colno++;
    }

    return result;
}

AggOperator *
agg_op_make(AggPlan *plan, OpChain *chain)
{
    AggOperator *agg_op;

    ASSERT(list_length(plan->plan.quals) == 0);

    agg_op = (AggOperator *) operator_make(OPER_AGG,
                                           sizeof(*agg_op),
                                           (PlanNode *) plan,
                                           NULL,
                                           chain,
                                           agg_invoke,
                                           agg_destroy);

    agg_op->num_aggs = count_agg_exprs(plan->head);
    agg_op->agg_info = make_agg_info(agg_op->num_aggs, plan->head,
                                     agg_op->op.pool);

    return agg_op;
}
