#include "col-internal.h"
#include "operator/operator.h"

Operator *
operator_make(OpKind op_kind, apr_size_t sz, op_invoke_func invoke_f,
              op_destroy_func destroy_f, apr_pool_t *pool)
{
    apr_pool_t *op_pool;
    Operator *op;

    op_pool = make_subpool(pool);
    op = apr_pcalloc(op_pool, sz);
    op->op_kind = op_kind;
    op->pool = op_pool;
    op->invoke = invoke_f;
    op->destroy = destroy_f;

    return op;
}

void
operator_destroy(Operator *op)
{
    apr_pool_destroy(op->pool);
}
