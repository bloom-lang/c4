#ifndef SCAN_H
#define SCAN_H

#include "operator/operator.h"
#include "planner/planner.h"
#include "storage/table.h"

typedef struct ScanOperator
{
    Operator op;
    int nquals;
    ExprState **qual_ary;
    C4Table *table;
} ScanOperator;

ScanOperator *scan_op_make(ScanPlan *plan, Operator *next_op, OpChain *chain);

#endif  /* SCAN_H */
