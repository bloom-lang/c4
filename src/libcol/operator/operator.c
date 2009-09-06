#include "col-internal.h"
#include "nodes/copyfuncs.h"
#include "operator/operator.h"

static apr_status_t operator_cleanup(void *data);

Operator *
operator_make(ColNodeKind kind, apr_size_t sz, PlanNode *plan,
              Operator *next_op, op_invoke_func invoke_f,
              op_destroy_func destroy_f, apr_pool_t *pool)
{
    Operator *op;
    ListCell *lc;
    int i;

    op = apr_pcalloc(pool, sz);
    op->node.kind = kind;
    op->pool = pool;
    op->plan = copy_node(plan, pool);
    op->next = next_op;
    op->exec_cxt = apr_pcalloc(pool, sizeof(*op->exec_cxt));
    op->invoke = invoke_f;
    op->destroy = destroy_f;

    op->nproj = list_length(op->plan->proj_list);
    op->proj_ary = apr_palloc(pool, sizeof(ExprState *) * op->nproj);

    i = 0;
    foreach (lc, op->plan->proj_list)
    {
        ExprNode *expr = (ExprNode *) lc_ptr(lc);

        op->proj_ary[i++] = make_expr_state(expr, op->exec_cxt, pool);
    }

    apr_pool_cleanup_register(pool, op, operator_cleanup,
                              apr_pool_cleanup_null);

    return op;
}

static apr_status_t
operator_cleanup(void *data)
{
    Operator *op = (Operator *) data;

    op->destroy(op);
    return APR_SUCCESS;
}

void
operator_destroy(Operator *op)
{
    ;
}
