#ifndef SQLITE_TABLE_H
#define SQLITE_TABLE_H

#include <sqlite3.h>

#include "storage/table.h"

typedef struct SQLiteTable
{
    AbstractTable table;
    sqlite3_stmt *insert_stmt;
} SQLiteTable;

SQLiteTable *sqlite_table_make(TableDef *def, C4Runtime *c4,
                               apr_pool_t *pool);

#endif  /* SQLITE_TABLE_H */
