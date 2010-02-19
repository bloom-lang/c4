#include "c4-internal.h"
#include "operator/agg.h"

static void
agg_invoke(Operator *op, Tuple *t)
{
    ;
}

static void
agg_destroy(Operator *op)
{
    operator_destroy(op);
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

    return agg_op;
}
