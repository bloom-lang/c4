#ifndef INSERT_H
#define INSERT_H

#include "operator/operator.h"
#include "planner/planner.h"
#include "storage/table.h"

typedef struct InsertOperator
{
    Operator op;
    TableDef *tbl_def;
} InsertOperator;

InsertOperator *insert_op_make(InsertPlan *plan, OpChain *chain);

#endif  /* INSERT_H */
