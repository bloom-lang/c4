#include "col-internal.h"
#include "operator/scan.h"

static void
scan_invoke(Operator *op, Tuple *t)
{
    ;
}

static void
scan_destroy(Operator *op)
{
    operator_destroy(op);
}

ScanOperator *
scan_op_make(apr_pool_t *pool)
{
    ScanOperator *scan_op;

    scan_op = (ScanOperator *) operator_make(OPER_SCAN, sizeof(*scan_op),
                                             scan_invoke, scan_destroy, pool);

    return scan_op;
}
