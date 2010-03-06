#include "c4-internal.h"
#include "operator/project.h"

static void
project_invoke(Operator *op, Tuple *t)
{
    ExprEvalContext *exec_cxt;
    Tuple *proj_tuple;

    exec_cxt = op->exec_cxt;
    exec_cxt->inner = t;

    proj_tuple = operator_do_project(op);
    op->next->invoke(op->next, proj_tuple);
    tuple_unpin(proj_tuple, op->proj_schema);
}

static void
project_destroy(Operator *op)
{
    operator_destroy(op);
}

ProjectOperator *
project_op_make(ProjectPlan *plan, Operator *next_op, OpChain *chain)
{
    ProjectOperator *proj_op;

    ASSERT(list_length(plan->plan.quals) == 0);

    proj_op = (ProjectOperator *) operator_make(OPER_PROJECT,
                                                sizeof(*proj_op),
                                                (PlanNode *) plan,
                                                next_op,
                                                chain,
                                                project_invoke,
                                                project_destroy);

    return proj_op;
}
