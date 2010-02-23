#ifndef MEM_TABLE_H
#define MEM_TABLE_H

#include "storage/table.h"
#include "util/rset.h"

typedef struct MemTable
{
    AbstractTable table;
    rset_t *tuples;
} MemTable;

MemTable *mem_table_make(TableDef *def, C4Runtime *c4, apr_pool_t *pool);

#endif  /* MEM_TABLE_H */
