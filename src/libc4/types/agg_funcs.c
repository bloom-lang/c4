#include "c4-internal.h"
#include "operator/agg.h"
#include "types/agg_funcs.h"
#include "util/rbtree.h"

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

/* Max */
struct max_tree_node
{
    RB_ENTRY(max_tree_node) entry;
    union
    {
        Datum val;                          /* Data value */
        struct max_tree_node *next_free;    /* Next free node, if on free list */
    } v;
};

typedef struct MaxStateVal MaxStateVal;

RB_HEAD(max_tree, max_tree_node, MaxStateVal *);

struct MaxStateVal
{
    apr_pool_t *pool;
    datum_cmp_func cmp_func;
    struct max_tree tree;
    struct max_tree_node *free_head;
};

static int
max_tree_node_cmp(struct max_tree *tree, struct max_tree_node *n1,
                  struct max_tree_node *n2)
{
    MaxStateVal *max_state = (MaxStateVal *) tree->opaque;

    return max_state->cmp_func(n1->v.val, n2->v.val);
}

RB_GENERATE_STATIC(max_tree, max_tree_node, entry, max_tree_node_cmp)

static void
max_insert_val(MaxStateVal *state, Datum v)
{
    struct max_tree_node *n;

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
    RB_INSERT(max_tree, &state->tree, n);
}

static AggStateVal
max_init_f(Datum v, AggOperator *agg_op, int aggno)
{
    AggStateVal state;
    MaxStateVal *max_state;
    apr_pool_t *max_pool;
    AggExprInfo *agg_info;
    int colno;
    DataType input_type;

    agg_info = agg_op->agg_info[aggno];
    colno = agg_info->colno;
    input_type = schema_get_type(agg_op->op.proj_schema, colno);

    max_pool = make_subpool(agg_op->op.pool);
    max_state = apr_palloc(max_pool, sizeof(*max_state));
    max_state->pool = max_pool;
    max_state->cmp_func = type_get_cmp_func(input_type);
    max_state->free_head = NULL;
    RB_INIT(&max_state->tree, max_state);

    max_insert_val(max_state, v);

    state.ptr = max_state;
    return state;
}

static AggStateVal
max_fw_trans_f(AggStateVal state, Datum v)
{
    MaxStateVal *max_state = (MaxStateVal *) state.ptr;

    max_insert_val(max_state, v);
    return state;
}

static AggStateVal
max_bw_trans_f(AggStateVal state, Datum v)
{
    MaxStateVal *max_state = (MaxStateVal *) state.ptr;
    struct max_tree_node find;
    struct max_tree_node *n;

    find.v.val = v;
    n = RB_FIND(max_tree, &max_state->tree, &find);
    RB_REMOVE(max_tree, &max_state->tree, n);

    n->v.next_free = max_state->free_head;
    max_state->free_head = n;

    return state;
}

static Datum
max_output_f(AggStateVal state)
{
    MaxStateVal *max_state = (MaxStateVal *) state.ptr;
    struct max_tree_node *n;

    n = RB_MAX(max_tree, &max_state->tree);
    return n->v.val;
}

static void
max_shutdown_f(AggStateVal state)
{
    MaxStateVal *max_state = (MaxStateVal *) state.ptr;

    apr_pool_destroy(max_state->pool);
}

/* Sum */
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

static AggFuncDesc agg_desc_count = {
    count_init_f,
    count_fw_trans_f,
    count_bw_trans_f,
    NULL,
    NULL
};

static AggFuncDesc agg_desc_max = {
    max_init_f,
    max_fw_trans_f,
    max_bw_trans_f,
    max_output_f,
    max_shutdown_f
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
        case AST_AGG_COUNT:
            return &agg_desc_count;

        case AST_AGG_MAX:
            return &agg_desc_max;

#if 0
        case AST_AGG_MIN:
            return &agg_desc_min;
#endif

        case AST_AGG_SUM:
            return &agg_desc_sum;

        default:
            ERROR("Unrecognized agg kind: %d", (int) agg_kind);
    }
}

