#include "c4-internal.h"
#include "operator/insert.h"
#include "router.h"

static void
insert_invoke(Operator *op, Tuple *t)
{
    C4Runtime *c4 = op->chain->c4;
    InsertOperator *insert_op = (InsertOperator *) op;

    if (router_is_deleting(c4->router))
        router_delete_tuple(c4->router, t, insert_op->tbl_def);
    else
        router_insert_tuple(c4->router, t, insert_op->tbl_def, true);
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
                                                 insert_invoke);

    insert_op->tbl_def = cat_get_table(chain->c4->cat, plan->head->name);

    return insert_op;
}
