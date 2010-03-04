#ifndef AGG_H
#define AGG_H

#include "operator/operator.h"
#include "types/agg_funcs.h"
#include "util/hash.h"
#include "util/rset.h"

typedef struct AggExprInfo
{
    int colno;
    AstAggKind agg_kind;
    AggFuncDesc *desc;
} AggExprInfo;

typedef struct AggGroupState
{
    Datum *trans_vals;
    struct AggGroupState *next;
} AggGroupState;

typedef struct AggOperator
{
    Operator op;
    int num_aggs;
    AggExprInfo **agg_info;
    int num_group_cols;
    int *group_colnos;
    rset_t *tuple_set;
    c4_hash_t *group_tbl;
    AggGroupState *free_groups;
} AggOperator;

AggOperator *agg_op_make(AggPlan *plan, OpChain *chain);

#endif  /* AGG_H */
