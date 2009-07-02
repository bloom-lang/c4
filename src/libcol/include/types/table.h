#ifndef TABLE_H
#define TABLE_H

#include "types/tuple.h"

typedef struct ColTable
{
    apr_pool_t *pool;
    char *name;
} ColTable;

ColTable *table_make(const char *name, apr_pool_t *pool);
void table_insert(ColTable *tbl, Tuple *t);

#endif  /* TABLE_H */
