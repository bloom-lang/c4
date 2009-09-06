#include "col-internal.h"
#include "operator/insert.h"

static void
insert_invoke(Operator *op, Tuple *t)
{
    InsertOperator *insert_op = (InsertOperator *) op;
}

static void
insert_destroy(Operator *op)
{
    operator_destroy(op);
}

InsertOperator *
insert_op_make(InsertPlan *plan, apr_pool_t *pool)
{
    InsertOperator *insert_op;

    ASSERT(list_length(plan->plan.quals) == 0);

    insert_op = (InsertOperator *) operator_make(OPER_INSERT,
                                                 sizeof(*insert_op),
                                                 (PlanNode *) plan,
                                                 NULL,
                                                 insert_invoke,
                                                 insert_destroy,
                                                 pool);

    return insert_op;
}
