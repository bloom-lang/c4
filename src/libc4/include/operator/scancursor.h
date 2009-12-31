#ifndef SCAN_CURSOR_H
#define SCAN_CURSOR_H

#include <sqlite3.h>

#include "operator/scan.h"
#include "util/hash.h"

/* only one of these cursor types should be non-null */
typedef struct ScanCursor
{
    c4_hash_index_t *hi;
    sqlite3_stmt *sqlite_stmt;
} ScanCursor;

#endif  /* SCAN_CURSOR_H */
