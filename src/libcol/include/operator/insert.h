#ifndef INSERT_H
#define INSERT_H

#include "operator/operator.h"
#include "planner/planner.h"

typedef struct InsertOperator
{
    Operator op;
    InsertOpPlan *plan;
} InsertOperator;

InsertOperator *insert_op_make(InsertOpPlan *plan, apr_pool_t *pool);

#endif  /* INSERT_H */
