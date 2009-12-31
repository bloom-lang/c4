#include "c4-internal.h"
#include "operator/scan.h"
#include "operator/scancursor.h"

static void
scan_invoke(Operator *op, Tuple *t)
{
    ScanOperator *scan_op = (ScanOperator *) op;
    AbstractTable *tbl = scan_op->table;
    ExprEvalContext *exec_cxt;
    Tuple *scan_tuple;
    ScanCursor cur;

    exec_cxt = scan_op->op.exec_cxt;
    exec_cxt->inner = t;

    for (scan_tuple = tbl->scan_first(tbl, &cur);
         scan_tuple != NULL;
         scan_tuple = tbl->scan_next(tbl, &cur))
    {
        exec_cxt->outer = scan_tuple;
        if (eval_qual_set(scan_op->nquals, scan_op->qual_ary))
        {
            Tuple *join_tuple;

            join_tuple = operator_do_project(op);
            op->next->invoke(op->next, join_tuple);
        }
    }
}

static void
scan_destroy(Operator *op)
{
    operator_destroy(op);
}

ScanOperator *
scan_op_make(ScanPlan *plan, Operator *next_op, OpChain *chain)
{
    ScanOperator *scan_op;
    char *tbl_name;
    ListCell *lc;
    int i;

    scan_op = (ScanOperator *) operator_make(OPER_SCAN,
                                             sizeof(*scan_op),
                                             (PlanNode *) plan,
                                             next_op,
                                             chain,
                                             scan_invoke,
                                             scan_destroy);

    tbl_name = plan->scan_rel->ref->name;
    scan_op->table = cat_get_table_impl(chain->c4->cat, tbl_name);

    scan_op->nquals = list_length(scan_op->op.plan->quals);
    scan_op->qual_ary = apr_palloc(scan_op->op.pool,
                                   sizeof(*scan_op->qual_ary) * scan_op->nquals);
    i = 0;
    foreach (lc, scan_op->op.plan->qual_exprs)
    {
        ExprNode *expr = (ExprNode *) lc_ptr(lc);

        scan_op->qual_ary[i++] = make_expr_state(expr,
                                                 scan_op->op.exec_cxt,
                                                 scan_op->op.pool);
    }

    return scan_op;
}
