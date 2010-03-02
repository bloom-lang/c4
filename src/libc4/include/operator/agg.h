#ifndef AGG_H
#define AGG_H

#include "operator/operator.h"
#include "util/hash.h"
#include "util/rset.h"

typedef struct AggExprInfo
{
    int colno;
    AstAggKind agg_kind;
} AggExprInfo;

typedef struct AggOperator
{
    Operator op;
    int num_aggs;
    AggExprInfo **agg_info;
    int num_group_cols;
    int *group_colnos;
    rset_t *tuple_set;
    c4_hash_t *group_tbl;
} AggOperator;

AggOperator *agg_op_make(AggPlan *plan, OpChain *chain);

#endif  /* AGG_H */
