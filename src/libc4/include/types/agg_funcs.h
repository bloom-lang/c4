#ifndef AGG_FUNCS_H
#define AGG_FUNCS_H

#include "parser/ast.h"

/*
 * Initialize the state of an agg, given an initial input value. If NULL, use
 * the initial value as the state.
 */
typedef Datum (*agg_init_f)(Datum v);

/* Forward or backward transition func: return updated state */
typedef Datum (*agg_trans_f)(Datum state, Datum v);

/* Emit an output value. If NULL, use the current state value */
typedef Datum (*agg_output_f)(Datum state);

/* Shutdown an aggregate. If NULL, shutdown is a no-op. */
typedef void (*agg_shutdown_f)(Datum state);

typedef struct AggFuncDesc
{
    agg_init_f init_f;
    agg_trans_f fw_trans_f;     /* Forward transition function */
    agg_trans_f bw_trans_f;     /* Backward transition function */
    agg_output_f output_f;
    agg_shutdown_f shutdown_f;
} AggFuncDesc;

AggFuncDesc *lookup_agg_desc(AstAggKind agg_kind);

#endif  /* AGG_FUNCS_H */
