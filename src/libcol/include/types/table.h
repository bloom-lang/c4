#ifndef TABLE_H
#define TABLE_H

#include "types/catalog.h"
#include "types/sqlite_table.h"
#include "types/tuple.h"
#include "util/hash.h"

typedef struct ColSQLiteTable
{
    sqlite3_stmt *insert_stmt;
    sqlite3_stmt *scan_stmt;
} ColSQLiteTable;

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
ColSQLiteTable *sqlite_table_make(ColTable *ctbl);
ColSQLiteTable *sqlite_table_open(const apr_file_t *fd, TableDef *def, ColInstance *col, apr_pool_t *pool);
bool sqlite_table_insert(ColTable *ctbl, Tuple *t);

#endif  /* TABLE_H */
