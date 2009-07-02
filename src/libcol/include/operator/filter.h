#ifndef FILTER_H
#define FILTER_H

#include "operator/operator.h"

typedef struct FilterOperator
{
    Operator op;
} FilterOperator;

FilterOperator *filter_op_make(apr_pool_t *pool);

#endif  /* FILTER_H */
