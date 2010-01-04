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
typedef struct ScanCursor *(*table_scan_make_f)(AbstractTable *tbl, apr_pool_t *pool);
typedef void (*table_scan_reset_f)(AbstractTable *tbl, struct ScanCursor *scan);
typedef Tuple *(*table_scan_next_f)(AbstractTable *tbl, struct ScanCursor *scan);

struct AbstractTable
{
    apr_pool_t *pool;
    C4Runtime *c4;
    TableDef *def;

    /* Table API */
    table_insert_f insert;
    table_cleanup_f cleanup;
    table_scan_make_f scan_make;
    table_scan_reset_f scan_reset;
    table_scan_next_f scan_next;
};

AbstractTable *table_make(TableDef *def, C4Runtime *c4, apr_pool_t *pool);
AbstractTable *table_make_super(apr_size_t sz, TableDef *def,
                                C4Runtime *c4, table_insert_f insert_f,
                                table_cleanup_f cleanup_f,
                                table_scan_make_f scan_make_f,
                                table_scan_reset_f scan_reset_f,
                                table_scan_next_f scan_next_f,
                                apr_pool_t *pool);

#endif  /* TABLE_H */
