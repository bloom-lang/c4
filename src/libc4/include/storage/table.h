#ifndef TABLE_H
#define TABLE_H

#include "types/catalog.h"
#include "types/tuple.h"

typedef struct AbstractTable AbstractTable;

struct ScanCursor;

/* Table API */

/*
 * Insert the tuple into this table. Returns "true" if the tuple was added;
 * returns false if the insert was a no-op because the tuple is already
 * contained by this table.
 */
typedef bool (*table_insert_f)(AbstractTable *tbl, Tuple *t);
typedef void (*table_cleanup_f)(AbstractTable *tbl);
typedef Tuple *(*table_scan_first_f)(AbstractTable *tbl, struct ScanCursor *cur);
typedef Tuple *(*table_scan_next_f)(AbstractTable *tbl, struct ScanCursor *cur);

struct AbstractTable
{
    apr_pool_t *pool;
    C4Instance *c4;
    TableDef *def;

    /* Table API */
    table_insert_f insert;
    table_cleanup_f cleanup;
    table_scan_first_f scan_first;
    table_scan_next_f scan_next;
};

AbstractTable *table_make(TableDef *def, C4Instance *c4, apr_pool_t *pool);
AbstractTable *table_make_super(apr_size_t sz, TableDef *def,
                                C4Instance *c4, table_insert_f insert_f,
                                table_cleanup_f cleanup_f,
                                table_scan_first_f scan_first_f,
                                table_scan_next_f scan_next_f,
                                apr_pool_t *pool);

#endif  /* TABLE_H */
