#include "col-internal.h"
#include "operator/filter.h"

static void
filter_invoke(Operator *op, Tuple *t)
{
    FilterOperator *filter_op = (FilterOperator *) op;
    ExprEvalContext *exec_cxt;

    exec_cxt = filter_op->op.exec_cxt;
    exec_cxt->inner = t;

    /* Only route the tuple onward if it passes all the quals */
    if (eval_qual_set(filter_op->nquals, filter_op->qual_ary))
        op->next->invoke(op->next, t);
}

static void
filter_destroy(Operator *op)
{
    operator_destroy(op);
}

FilterOperator *
filter_op_make(FilterPlan *plan, Operator *next_op, apr_pool_t *pool)
{
    FilterOperator *filter_op;
    ListCell *lc;
    int i;

    filter_op = (FilterOperator *) operator_make(OPER_FILTER,
                                                 sizeof(*filter_op),
                                                 (PlanNode *) plan,
                                                 next_op,
                                                 filter_invoke,
                                                 filter_destroy,
                                                 pool);

    filter_op->nquals = list_length(filter_op->op.plan->quals);
    i = 0;
    foreach (lc, filter_op->op.plan->quals)
    {
        ExprNode *expr = (ExprNode *) lc_ptr(lc);

        filter_op->qual_ary[i++] = make_expr_state(expr,
                                                   filter_op->op.exec_cxt,
                                                   filter_op->op.pool);
    }

    return filter_op;
}
