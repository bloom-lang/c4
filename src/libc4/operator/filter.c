#include "c4-internal.h"
#include "operator/filter.h"

static void
filter_invoke(Operator *op, Tuple *t)
{
    FilterOperator *filter_op = (FilterOperator *) op;
    ExprEvalContext *exec_cxt;

    exec_cxt = filter_op->op.exec_cxt;
    exec_cxt->inner = t;

    /*
     * Only route the tuple onward if it passes all the quals. Note that
     * although OPER_FILTER has a projection list, we don't actually do any
     * projection: the planner ensures that the associated projection list
     * just preserves the input to the filter.
     */
    if (eval_qual_set(filter_op->nquals, filter_op->qual_ary))
        op->next->invoke(op->next, t);
}

static void
filter_destroy(Operator *op)
{
    operator_destroy(op);
}

FilterOperator *
filter_op_make(FilterPlan *plan, Operator *next_op, OpChain *chain)
{
    FilterOperator *filter_op;
    ListCell *lc;
    int i;

    filter_op = (FilterOperator *) operator_make(OPER_FILTER,
                                                 sizeof(*filter_op),
                                                 (PlanNode *) plan,
                                                 next_op,
                                                 chain,
                                                 filter_invoke,
                                                 filter_destroy);

    filter_op->nquals = list_length(filter_op->op.plan->quals);
    filter_op->qual_ary = apr_palloc(filter_op->op.pool,
                                     sizeof(*filter_op->qual_ary) * filter_op->nquals);
    i = 0;
    foreach (lc, filter_op->op.plan->qual_exprs)
    {
        ExprNode *expr = (ExprNode *) lc_ptr(lc);

        filter_op->qual_ary[i++] = make_expr_state(expr,
                                                   filter_op->op.exec_cxt,
                                                   filter_op->op.pool);
    }

    return filter_op;
}
