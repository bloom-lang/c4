#ifndef TABLE_H
#define TABLE_H

#include "types/catalog.h"
#include "types/tuple.h"
#include "util/hash.h"

typedef struct ColSQLiteTable ColSQLiteTable;

typedef struct ColTable
{
    apr_pool_t *pool;
    ColInstance *col;
    TableDef *def;
    col_hash_t *tuples;
    ColSQLiteTable *sql_table;
} ColTable;

ColTable *table_make(TableDef *def, ColInstance *col, apr_pool_t *pool, bool sql);
bool table_insert(ColTable *tbl, Tuple *t);

#endif  /* TABLE_H */
