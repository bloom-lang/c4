#include "c4-internal.h"
#include "types/agg_funcs.h"

static Datum
count_init_f(Datum v)
{
    Datum state;

    state.i8 = 0;
    return state;
}

static Datum
count_fw_trans_f(Datum state, Datum v)
{
    state.i8++;
    return state;
}

static Datum
count_bw_trans_f(Datum state, Datum v)
{
    state.i8--;
    return state;
}

static Datum
sum_fw_trans_f(Datum state, Datum v)
{
    state.i8 += v.i8;
    return state;
}

static Datum
sum_bw_trans_f(Datum state, Datum v)
{
    state.i8 -= v.i8;
    return state;
}

static AggFuncDesc agg_desc_count = {
    count_init_f,
    count_fw_trans_f,
    count_bw_trans_f,
    NULL,
    NULL
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
#if 0
        case AST_AGG_MAX:
            return &agg_desc_max;

        case AST_AGG_MIN:
            return &agg_desc_min;
#endif

        case AST_AGG_SUM:
            return &agg_desc_sum;

        default:
            ERROR("Unrecognized agg kind: %d", (int) agg_kind);
    }
}

