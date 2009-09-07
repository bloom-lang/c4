#include "col-internal.h"
#include "operator/insert.h"

static void
insert_invoke(Operator *op, Tuple *t)
{
    InsertOperator *insert_op = (InsertOperator *) op;
    ExprEvalContext *exec_cxt;
    Tuple *proj_tuple;
    char *str;

    exec_cxt = insert_op->op.exec_cxt;
    exec_cxt->inner = t;
    proj_tuple = operator_do_project(op);

    /* XXX */
    str = tuple_to_str(proj_tuple, op->pool);
    printf("INSERT: %s\n", str);
}

static void
insert_destroy(Operator *op)
{
    operator_destroy(op);
}

InsertOperator *
insert_op_make(InsertPlan *plan, OpChain *chain)
{
    InsertOperator *insert_op;

    ASSERT(list_length(plan->plan.quals) == 0);

    insert_op = (InsertOperator *) operator_make(OPER_INSERT,
                                                 sizeof(*insert_op),
                                                 (PlanNode *) plan,
                                                 NULL,
                                                 chain,
                                                 insert_invoke,
                                                 insert_destroy);

    return insert_op;
}
