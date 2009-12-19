#ifndef SQLITE_SCAN_H
#define SQLITE_SCAN_H

#include "operator/operator.h"
#include "planner/planner.h"
#include "types/table.h"

typedef struct SQLiteScanOperator
{
    Operator op;
    int nquals;
    ExprState **qual_ary;
    ColSQLiteTable *table;
} SQLiteScanOperator;

ScanOperator *sqlite_scan_op_make(ScanPlan *plan, Operator *next_op, OpChain *chain);

#endif  /* SQLITE_SCAN_H */
