#ifndef TABLE_H
#define TABLE_H

#include "types/tuple.h"
#include "util/hash.h"

typedef struct ColTable
{
    apr_pool_t *pool;
    char *name;
    col_hash_t *tuples;
} ColTable;

ColTable *table_make(const char *name, apr_pool_t *pool);
bool table_insert(ColTable *tbl, Tuple *t);

#endif  /* TABLE_H */
