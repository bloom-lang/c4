#ifndef FILTER_H
#define FILTER_H

#include "operator/operator.h"
#include "planner/planner.h"

typedef struct FilterOperator
{
    Operator op;
    FilterPlan *plan;
} FilterOperator;

FilterOperator *filter_op_make(FilterPlan *plan, Operator *next_op,
                               apr_pool_t *pool);

#endif  /* FILTER_H */
