#ifndef TABLE_H
#define TABLE_H

#include "types/catalog.h"
#include "types/tuple.h"
#include "util/hash.h"

typedef struct ColTable
{
    apr_pool_t *pool;
    ColInstance *col;
    TableDef *def;
    col_hash_t *tuples;
} ColTable;

ColTable *table_make(TableDef *def, ColInstance *col, apr_pool_t *pool);
bool table_insert(ColTable *tbl, Tuple *t);

#endif  /* TABLE_H */
