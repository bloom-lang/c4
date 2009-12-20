#ifndef SQLITE_TABLE_H
#define SQLITE_TABLE_H

#include "storage/table.h"

struct ColSQLiteTable
{
    sqlite3_stmt *insert_stmt;
    sqlite3_stmt *scan_stmt;
};

ColSQLiteTable *sqlite_table_make(ColTable *ctbl);
bool sqlite_table_insert(ColTable *ctbl, Tuple *t);

#endif  /* SQLITE_TABLE_H */
