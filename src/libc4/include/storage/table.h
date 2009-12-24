#ifndef TABLE_H
#define TABLE_H

#include "types/catalog.h"
#include "types/tuple.h"
#include "util/hash.h"

typedef struct C4SQLiteTable C4SQLiteTable;

typedef struct C4Table
{
    apr_pool_t *pool;
    C4Instance *c4;
    TableDef *def;
    c4_hash_t *tuples;
    C4SQLiteTable *sql_table;
} C4Table;

C4Table *table_make(TableDef *def, C4Instance *c4, apr_pool_t *pool);
bool table_insert(C4Table *tbl, Tuple *t);

#endif  /* TABLE_H */
