#include "col-internal.h"
#include "operator/operator.h"

static apr_status_t operator_cleanup(void *data);

Operator *
operator_make(OpKind op_kind, apr_size_t sz, op_invoke_func invoke_f,
              op_destroy_func destroy_f, apr_pool_t *pool)
{
    Operator *op;

    op = apr_pcalloc(pool, sz);
    op->op_kind = op_kind;
    op->pool = pool;
    op->invoke = invoke_f;
    op->destroy = destroy_f;

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
