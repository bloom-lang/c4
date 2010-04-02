#include "c4-internal.h"
#include "operator/agg.h"
#include "types/agg_funcs.h"

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
typedef struct MaxStateVal
{
    apr_pool_t *pool;
    datum_cmp_func cmp_func;
    Datum max;
} MaxStateVal;

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
    max_state->max = v;

    state.ptr = max_state;
    return state;
}

static AggStateVal
max_fw_trans_f(AggStateVal state, Datum v)
{
    return state;
}

static AggStateVal
max_bw_trans_f(AggStateVal state, Datum v)
{
    return state;
}

static Datum
max_output_f(AggStateVal state)
{
    MaxStateVal *max_state = (MaxStateVal *) state.ptr;

    return max_state->max;
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

