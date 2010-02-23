#ifndef SCAN_CURSOR_H
#define SCAN_CURSOR_H

#include <sqlite3.h>

#include "util/rset.h"

typedef struct ScanCursor
{
    apr_pool_t *pool;
    /* Cursor over MemTable */
    rset_index_t *rset_iter;
    /* Cursor over SQLiteTable */
    sqlite3_stmt *sqlite_stmt;
} ScanCursor;

#endif  /* SCAN_CURSOR_H */
