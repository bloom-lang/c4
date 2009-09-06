#include "col-internal.h"
#include "operator/scan.h"

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
scan_op_make(ScanPlan *plan, Operator *next_op, apr_pool_t *pool)
{
    ScanOperator *scan_op;
    ListCell *lc;
    int i;

    scan_op = (ScanOperator *) operator_make(OPER_SCAN,
                                             sizeof(*scan_op),
                                             (PlanNode *) plan,
                                             next_op,
                                             scan_invoke,
                                             scan_destroy,
                                             pool);

    scan_op->nquals = list_length(scan_op->op.plan->quals);
    scan_op->qual_ary = apr_palloc(scan_op->op.pool,
                                   sizeof(*scan_op->qual_ary) * scan_op->nquals);
    i = 0;
    foreach (lc, scan_op->op.plan->quals)
    {
        ExprNode *expr = (ExprNode *) lc_ptr(lc);

        scan_op->qual_ary[i++] = make_expr_state(expr,
                                                 scan_op->op.exec_cxt,
                                                 scan_op->op.pool);
    }

    return scan_op;
}
