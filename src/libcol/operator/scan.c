#include "col-internal.h"
#include "operator/scan.h"
#include "parser/copyfuncs.h"

static void
scan_invoke(Operator *op, Tuple *t)
{
    ScanOperator *scan_op = (ScanOperator *) op;
}

static void
scan_destroy(Operator *op)
{
    operator_destroy(op);
}

ScanOperator *
scan_op_make(ScanOpPlan *plan, apr_pool_t *pool)
{
    ScanOperator *scan_op;

    scan_op = (ScanOperator *) operator_make(OPER_SCAN, sizeof(*scan_op),
                                             scan_invoke, scan_destroy, pool);
    scan_op->plan = copy_node(plan, pool);

    return scan_op;
}
