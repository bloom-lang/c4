#ifndef AGG_TABLE_H
#define AGG_TABLE_H

#include "storage/table.h"

typedef struct AggTable
{
    AbstractTable table;
} AggTable;

AggTable *agg_table_make(TableDef *def, C4Runtime *c4, apr_pool_t *pool);

#endif  /* AGG_TABLE_H */
