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

    exec_cxt = scan_op->op.exec_cxt;
    exec_cxt->inner = t;

    tbl->scan_reset(tbl, scan_op->cursor);
    while ((scan_tuple = tbl->scan_next(tbl, scan_op->cursor)) != NULL)
    {
        exec_cxt->outer = scan_tuple;

        if (eval_qual_set(scan_op->nquals, scan_op->qual_ary))
        {
            Tuple *join_tuple;

            /* If this is NOT and we see a matching tuple, we're done */
            if (scan_op->anti_scan)
                return;

            join_tuple = operator_do_project(op);
            op->next->invoke(op->next, join_tuple);
        }
    }

    /* If this is NOT and no matches, emit an output tuple */
    if (scan_op->anti_scan)
    {
        Tuple *join_tuple;

        exec_cxt->outer = NULL;
        join_tuple = operator_do_project(op);
        op->next->invoke(op->next, join_tuple);
    }
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
                                             scan_invoke);

    tbl_name = plan->scan_rel->ref->name;
    scan_op->table = cat_get_table_impl(chain->c4->cat, tbl_name);
    scan_op->cursor = scan_op->table->scan_make(scan_op->table, scan_op->op.pool);
    scan_op->anti_scan = plan->scan_rel->not;

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
