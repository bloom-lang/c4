#include "c4-internal.h"
#include "operator/agg.h"
#include "types/agg_funcs.h"
#include "util/rbtree.h"

/* Average */
typedef struct AvgStateVal
{
    apr_pool_t *pool;
    Datum sum;
    Datum count;
} AvgStateVal;

/* XXX: Currently assume that avg input and output is TYPE_DOUBLE */
static AggStateVal
avg_init_f(Datum v, AggOperator *agg_op, __unused int aggno)
{
    apr_pool_t *pool;
    AvgStateVal *avg_state;
    AggStateVal state;

    pool = make_subpool(agg_op->op.pool);
    avg_state = apr_palloc(pool, sizeof(*avg_state));
    avg_state->pool = pool;
    avg_state->count.i8 = 1;
    avg_state->sum.d8 = v.d8;

    state.ptr = avg_state;
    return state;
}

static AggStateVal
avg_fw_trans_f(AggStateVal state, Datum v)
{
    AvgStateVal *avg_state = (AvgStateVal *) state.ptr;

    avg_state->count.i8++;
    avg_state->sum.d8 += v.d8;

    return state;
}

static AggStateVal
avg_bw_trans_f(AggStateVal state, Datum v)
{
    AvgStateVal *avg_state = (AvgStateVal *) state.ptr;

    avg_state->count.i8--;
    avg_state->sum.d8 -= v.d8;

    return state;
}

static Datum
avg_output_f(AggStateVal state)
{
    AvgStateVal *avg_state = (AvgStateVal *) state.ptr;
    Datum result;

    result.d8 = avg_state->sum.d8 / (double) avg_state->count.i8;
    return result;
}

static void
avg_shutdown_f(AggStateVal state)
{
    AvgStateVal *avg_state = (AvgStateVal *) state.ptr;

    apr_pool_destroy(avg_state->pool);
}

/* Count */
static AggStateVal
count_init_f(__unused Datum v, __unused AggOperator *agg_op, __unused int aggno)
{
    AggStateVal state;

    state.d.i8 = 1;
    return state;
}

static AggStateVal
count_fw_trans_f(AggStateVal state, __unused Datum v)
{
    state.d.i8++;
    return state;
}

static AggStateVal
count_bw_trans_f(AggStateVal state, __unused Datum v)
{
    state.d.i8--;
    return state;
}

/*
 * Min and Max. For both these aggregates, we keep the input we've seen so far
 * in a red-black tree. The only difference between min and max is that the
 * output func for min returns RB_MIN of the tree, while max does RB_MAX. Hence,
 * we call the common code "extrema".
 */
struct extrema_node
{
    RB_ENTRY(extrema_node) entry;
    union
    {
        Datum val;                          /* Data value */
        struct extrema_node *next_free;    /* Next free node, if on free list */
    } v;
};

typedef struct ExtremaStateVal ExtremaStateVal;

RB_HEAD(extrema_tree, extrema_node, ExtremaStateVal *);

struct ExtremaStateVal
{
    apr_pool_t *pool;
    datum_cmp_func cmp_func;
    struct extrema_tree tree;
    struct extrema_node *free_head;
};

static int
extrema_node_cmp(struct extrema_tree *tree, struct extrema_node *n1,
                 struct extrema_node *n2)
{
    ExtremaStateVal *extrema_state = (ExtremaStateVal *) tree->opaque;

    return extrema_state->cmp_func(n1->v.val, n2->v.val);
}

RB_GENERATE_STATIC(extrema_tree, extrema_node, entry, extrema_node_cmp)

static void
extrema_insert_val(ExtremaStateVal *state, Datum v)
{
    struct extrema_node *n;

    if (state->free_head)
    {
        n = state->free_head;
        state->free_head = n->v.next_free;
    }
    else
    {
        n = apr_palloc(state->pool, sizeof(*n));
    }

    n->v.val = v;
    RB_INSERT(extrema_tree, &state->tree, n);
}

static AggStateVal
extrema_init_f(Datum v, AggOperator *agg_op, int aggno)
{
    AggStateVal state;
    ExtremaStateVal *ext_state;
    apr_pool_t *pool;
    AggExprInfo *agg_info;
    int colno;
    DataType input_type;

    agg_info = agg_op->agg_info[aggno];
    colno = agg_info->colno;
    input_type = schema_get_type(agg_op->op.proj_schema, colno);

    pool = make_subpool(agg_op->op.pool);
    ext_state = apr_palloc(pool, sizeof(*ext_state));
    ext_state->pool = pool;
    ext_state->cmp_func = type_get_cmp_func(input_type);
    ext_state->free_head = NULL;
    RB_INIT(&ext_state->tree, ext_state);

    extrema_insert_val(ext_state, v);

    state.ptr = ext_state;
    return state;
}

static AggStateVal
extrema_fw_trans_f(AggStateVal state, Datum v)
{
    ExtremaStateVal *ext_state = (ExtremaStateVal *) state.ptr;

    extrema_insert_val(ext_state, v);
    return state;
}

static AggStateVal
extrema_bw_trans_f(AggStateVal state, Datum v)
{
    ExtremaStateVal *ext_state = (ExtremaStateVal *) state.ptr;
    struct extrema_node find;
    struct extrema_node *n;

    find.v.val = v;
    n = RB_FIND(extrema_tree, &ext_state->tree, &find);
    RB_REMOVE(extrema_tree, &ext_state->tree, n);

    n->v.next_free = ext_state->free_head;
    ext_state->free_head = n;

    return state;
}

static Datum
max_output_f(AggStateVal state)
{
    ExtremaStateVal *ext_state = (ExtremaStateVal *) state.ptr;
    struct extrema_node *n;

    n = RB_MAX(extrema_tree, &ext_state->tree);
    return n->v.val;
}

static Datum
min_output_f(AggStateVal state)
{
    ExtremaStateVal *ext_state = (ExtremaStateVal *) state.ptr;
    struct extrema_node *n;

    n = RB_MIN(extrema_tree, &ext_state->tree);
    return n->v.val;
}

static void
extrema_shutdown_f(AggStateVal state)
{
    ExtremaStateVal *ext_state = (ExtremaStateVal *) state.ptr;

    apr_pool_destroy(ext_state->pool);
}

/* Sum */
/* XXX: Currently assume that sum input and output is TYPE_INT8 */
static AggStateVal
sum_fw_trans_f(AggStateVal state, Datum v)
{
    state.d.i8 += v.i8;
    return state;
}

static AggStateVal
sum_bw_trans_f(AggStateVal state, Datum v)
{
    state.d.i8 -= v.i8;
    return state;
}

static AggFuncDesc agg_desc_avg = {
    avg_init_f,
    avg_fw_trans_f,
    avg_bw_trans_f,
    avg_output_f,
    avg_shutdown_f
};

static AggFuncDesc agg_desc_count = {
    count_init_f,
    count_fw_trans_f,
    count_bw_trans_f,
    NULL,
    NULL
};

static AggFuncDesc agg_desc_max = {
    extrema_init_f,
    extrema_fw_trans_f,
    extrema_bw_trans_f,
    max_output_f,
    extrema_shutdown_f
};

static AggFuncDesc agg_desc_min = {
    extrema_init_f,
    extrema_fw_trans_f,
    extrema_bw_trans_f,
    min_output_f,
    extrema_shutdown_f
};

static AggFuncDesc agg_desc_sum = {
    NULL,
    sum_fw_trans_f,
    sum_bw_trans_f,
    NULL,
    NULL
};

AggFuncDesc *
lookup_agg_desc(AstAggKind agg_kind)
{
    switch (agg_kind)
    {
        case AST_AGG_AVG:
            return &agg_desc_avg;

        case AST_AGG_COUNT:
            return &agg_desc_count;

        case AST_AGG_MAX:
            return &agg_desc_max;

        case AST_AGG_MIN:
            return &agg_desc_min;

        case AST_AGG_SUM:
            return &agg_desc_sum;

        default:
            ERROR("Unrecognized agg kind: %d", (int) agg_kind);
    }
}

