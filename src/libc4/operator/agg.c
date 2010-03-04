#include "c4-internal.h"
#include "operator/agg.h"
#include "router.h"

static bool add_new_tuple(Tuple *t, AggOperator *agg_op);
static void agg_do_delete(Tuple *t, AggOperator *agg_op);
static void agg_do_insert(Tuple *t, AggOperator *agg_op);

static void
agg_invoke(Operator *op, Tuple *t)
{
    AggOperator *agg_op = (AggOperator *) op;
    C4Runtime *c4 = op->chain->c4;
    ExprEvalContext *exec_cxt;
    Tuple *proj_tuple;
    bool need_work;

    exec_cxt = agg_op->op.exec_cxt;
    exec_cxt->inner = t;

    proj_tuple = operator_do_project(op);
    need_work = add_new_tuple(proj_tuple, agg_op);
    if (need_work)
    {
        if (router_is_deleting(c4->router))
            agg_do_delete(proj_tuple, agg_op);
        else
            agg_do_insert(proj_tuple, agg_op);
    }

    tuple_unpin(proj_tuple, op->proj_schema);
}

static bool
add_new_tuple(Tuple *t, AggOperator *agg_op)
{
    C4Runtime *c4 = agg_op->op.chain->c4;

    if (router_is_deleting(c4->router))
    {
        bool is_new;

        is_new = rset_add(agg_op->tuple_set, t);
        if (is_new)
            tuple_pin(t);

        return is_new;
    }
    else
    {
        Tuple *old_t;
        unsigned int new_count;

        old_t = rset_remove(agg_op->tuple_set, t, &new_count);
        if (old_t != NULL && new_count == 0)
        {
            tuple_unpin(old_t, agg_op->op.proj_schema);
            return true;
        }

        return false;
    }
}

static void
create_agg_group(Tuple *t, AggOperator *agg_op)
{
    AggGroupState *new_group;
    int i;

    if (agg_op->free_groups != NULL)
    {
        new_group = agg_op->free_groups;
        agg_op->free_groups = new_group->next;
    }
    else
    {
        new_group = apr_palloc(agg_op->op.pool, sizeof(*new_group));
        new_group->trans_vals = apr_palloc(agg_op->op.pool,
                                           sizeof(Datum) * agg_op->num_aggs);
    }

    new_group->count = 1;
    for (i = 0; i < agg_op->num_aggs; i++)
    {
        AggExprInfo *agg_info;
        Datum input_val;

        agg_info = agg_op->agg_info[i];
        input_val = tuple_get_val(t, agg_info->colno);
        new_group->trans_vals[i] = agg_info->desc->init_f(input_val);
    }

    c4_hash_set(agg_op->group_tbl, t, new_group);
    tuple_pin(t);

    /* XXX: Route new agg value */
}

static void
remove_agg_group(Tuple *t, AggGroupState *agg_group, AggOperator *agg_op)
{
    ;
}

static void
advance_agg_group(Tuple *t, AggGroupState *agg_group, AggOperator *agg_op)
{
    ;
}

static void
agg_do_delete(Tuple *t, AggOperator *agg_op)
{
    AggGroupState *agg_group;

    agg_group = c4_hash_get(agg_op->group_tbl, t);
    if (agg_group == NULL)
        return;

    agg_group->count--;
    if (agg_group->count == 0)
    {
        remove_agg_group(t, agg_group, agg_op);
        return;
    }

    advance_agg_group(t, agg_group, agg_op);
}

static void
agg_do_insert(Tuple *t, AggOperator *agg_op)
{
    AggGroupState *agg_group;

    agg_group = c4_hash_get(agg_op->group_tbl, t);
    if (agg_group == NULL)
    {
        create_agg_group(t, agg_op);
        return;
    }

    agg_group->count++;
    advance_agg_group(t, agg_group, agg_op);
}

static void
agg_destroy(Operator *op)
{
    operator_destroy(op);
}

static unsigned int
group_tbl_hash(const char *key, int klen, void *data)
{
    Tuple *t = (Tuple *) key;
    AggOperator *agg_op = (AggOperator *) data;
    int i;
    unsigned int result;

    ASSERT(klen == sizeof(Tuple *));
    result = 37;
    for (i = 0; i < agg_op->num_group_cols; i++)
    {
        int colno;
        Datum val;
        apr_uint32_t h;

        colno = agg_op->group_colnos[i];
        val = tuple_get_val(t, colno);
        h = (agg_op->op.proj_schema->hash_funcs[colno])(val);
        result ^= h;
    }

    return result;
}

static bool
group_tbl_cmp(const void *k1, const void *k2, int klen, void *data)
{
    Tuple *t1 = (Tuple *) k1;
    Tuple *t2 = (Tuple *) k2;
    AggOperator *agg_op = (AggOperator *) data;
    int i;

    ASSERT(klen == sizeof(Tuple *));
    for (i = 0; i < agg_op->num_group_cols; i++)
    {
        int colno;
        Datum val1;
        Datum val2;
        bool result;

        colno = agg_op->group_colnos[i];
        val1 = tuple_get_val(t1, colno);
        val2 = tuple_get_val(t2, colno);
        result = (agg_op->op.proj_schema->eq_funcs[colno])(val1, val2);
        if (!result)
            return false;
    }

    return true;
}

/*
 * We assume that AggExprs appear as top-level operators (i.e. aggs cannot be
 * nested within expression trees); the analysis phase currently enforces
 * this. Note that this only works with the AST representation of expressions:
 * see make_eval_expr().
 */
static int
count_agg_exprs(AstTableRef *head)
{
    int num_aggs;
    ListCell *lc;

    num_aggs = 0;
    foreach (lc, head->cols)
    {
        C4Node *expr = (C4Node *) lc_ptr(lc);

        if (expr->kind == AST_AGG_EXPR)
            num_aggs++;
    }

    ASSERT(num_aggs > 0);
    return num_aggs;
}

static AggExprInfo **
make_agg_info(int num_aggs, List *cols, apr_pool_t *pool)
{
    AggExprInfo **result;
    ListCell *lc;
    int aggno;
    int colno;

    result = apr_palloc(pool, num_aggs * sizeof(*result));
    aggno = 0;
    colno = 0;
    foreach (lc, cols)
    {
        C4Node *expr = (C4Node *) lc_ptr(lc);
        AstAggExpr *agg_expr;
        AggExprInfo *agg_info;

        if (expr->kind != AST_AGG_EXPR)
        {
            colno++;
            continue;
        }

        agg_expr = (AstAggExpr *) expr;
        agg_info = apr_palloc(pool, sizeof(*agg_info));
        agg_info->colno = colno;
        agg_info->agg_kind = agg_expr->agg_kind;
        agg_info->desc = lookup_agg_desc(agg_info->agg_kind);
        result[aggno] = agg_info;
        aggno++;
        colno++;
    }

    return result;
}

static int *
make_group_colnos(int num_group_cols, List *cols, apr_pool_t *pool)
{
    ListCell *lc;
    int *colno_ary;
    int colno = 0;
    int idx = 0;

    colno_ary = apr_palloc(pool, num_group_cols * sizeof(*colno_ary));
    foreach (lc, cols)
    {
        C4Node *expr = (C4Node *) lc_ptr(lc);

        if (expr->kind == AST_AGG_EXPR)
        {
            colno++;
            continue;
        }

        colno_ary[idx] = colno;
        colno++;
        idx++;
    }

    return colno_ary;
}

AggOperator *
agg_op_make(AggPlan *plan, OpChain *chain)
{
    AggOperator *agg_op;
    int num_cols;

    ASSERT(list_length(plan->plan.quals) == 0);

    agg_op = (AggOperator *) operator_make(OPER_AGG,
                                           sizeof(*agg_op),
                                           (PlanNode *) plan,
                                           NULL,
                                           chain,
                                           agg_invoke,
                                           agg_destroy);

    agg_op->num_aggs = count_agg_exprs(plan->head);
    agg_op->agg_info = make_agg_info(agg_op->num_aggs, plan->head->cols,
                                     agg_op->op.pool);

    num_cols = list_length(plan->head->cols);
    ASSERT(num_cols >= agg_op->num_aggs);
    agg_op->num_group_cols = num_cols - agg_op->num_aggs;
    agg_op->group_colnos = make_group_colnos(agg_op->num_group_cols,
                                             plan->head->cols, agg_op->op.pool);

    agg_op->group_tbl = c4_hash_make(agg_op->op.pool, sizeof(Tuple *),
                                     agg_op, group_tbl_hash, group_tbl_cmp);
    agg_op->tuple_set = rset_make(agg_op->op.pool, agg_op->op.proj_schema,
                                  tuple_hash_tbl, tuple_cmp_tbl);
    agg_op->free_groups = NULL;

    return agg_op;
}
