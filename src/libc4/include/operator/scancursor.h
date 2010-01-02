#ifndef SCAN_CURSOR_H
#define SCAN_CURSOR_H

#include <sqlite3.h>

#include "util/hash.h"

typedef struct ScanCursor
{
    apr_pool_t *pool;
    /* Cursor over MemTable */
    c4_hash_index_t *hash_iter;
    /* Cursor over SQLiteTable */
    sqlite3_stmt *sqlite_stmt;
} ScanCursor;

#endif  /* SCAN_CURSOR_H */
