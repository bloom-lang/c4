#ifndef SQLITE_TABLE_H
#define SQLITE_TABLE_H

#include "storage/table.h"

struct C4SQLiteTable
{
    sqlite3_stmt *insert_stmt;
    sqlite3_stmt *scan_stmt;
};

C4SQLiteTable *sqlite_table_make(C4Table *ctbl);
bool sqlite_table_insert(C4Table *ctbl, Tuple *t);

#endif  /* SQLITE_TABLE_H */
