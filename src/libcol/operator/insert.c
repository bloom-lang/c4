#include "col-internal.h"
#include "operator/insert.h"
#include "parser/copyfuncs.h"

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
insert_op_make(InsertOpPlan *plan, apr_pool_t *pool)
{
    InsertOperator *insert_op;

    insert_op = (InsertOperator *) operator_make(OPER_INSERT,
                                                 sizeof(*insert_op),
                                                 NULL,
                                                 insert_invoke,
                                                 insert_destroy,
                                                 pool);
    insert_op->plan = copy_node(plan, pool);

    return insert_op;
}