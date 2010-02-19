#ifndef AGG_H
#define AGG_H

#include "operator/operator.h"

typedef struct AggOperator
{
    Operator op;
} AggOperator;

AggOperator *agg_op_make(AggPlan *plan, OpChain *chain);

#endif  /* AGG_H */
