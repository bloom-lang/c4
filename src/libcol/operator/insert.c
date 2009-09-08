#include "col-internal.h"
#include "operator/insert.h"
#include "router.h"

static void
insert_invoke(Operator *op, Tuple *t)
{
    InsertOperator *insert_op = (InsertOperator *) op;
    ExprEvalContext *exec_cxt;
    Tuple *proj_tuple;

    exec_cxt = insert_op->op.exec_cxt;
    exec_cxt->inner = t;
    proj_tuple = operator_do_project(op);

    if (table_insert(insert_op->tbl_def->table, proj_tuple))
    {
        router_enqueue_internal(op->chain->col->router,
                                proj_tuple, insert_op->tbl_def);
    }

    /* XXX debug */
    col_log(op->chain->col, "INSERT => %s: %s",
            insert_op->tbl_def->name,
            log_tuple(op->chain->col, proj_tuple));
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

    insert_op->tbl_def = cat_get_table(chain->col->cat, plan->head->name);

    return insert_op;
}
