#ifndef AGG_H
#define AGG_H

#include "operator/operator.h"
#include "types/agg_funcs.h"
#include "types/catalog.h"
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
    /*
     * We keep a pointer (+ refcount pin) to the first tuple that was inserted
     * into the group; this forms the key for the group hash table.  Annoyingly,
     * we also need to keep a pointer to the key in the group state itself, so
     * that we can release the pin when the group is removed.
     */
    Tuple *key;
    Tuple *output_tup;
    int count;
    Datum *state_vals;
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
    TableDef *output_tbl;
} AggOperator;

AggOperator *agg_op_make(AggPlan *plan, OpChain *chain);

#endif  /* AGG_H */
