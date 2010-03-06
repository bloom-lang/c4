#ifndef PROJECT_H
#define PROJECT_H

#include "operator/operator.h"
#include "planner/planner.h"

typedef struct ProjectOperator
{
    Operator op;
} ProjectOperator;

ProjectOperator *project_op_make(ProjectPlan *plan, Operator *next_op,
                                 OpChain *chain);

#endif  /* PROJECT_H */
