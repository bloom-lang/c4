#ifndef SCAN_CURSOR_H
#define SCAN_CURSOR_H

#include "operator/scan.h"

/* only one of these cursor types should be non-null */
typedef struct ScanCursor
{
    c4_hash_index_t *hi;
    sqlite3_stmt *sqlite_stmt;
} ScanCursor;

Tuple *table_scan_first(C4Table *tbl, ScanCursor *cur);
Tuple *table_scan_next(C4Table *tbl, ScanCursor *cur);
Tuple *sqlite_table_scan_first(C4Table *tbl, ScanCursor *cur);
Tuple *sqlite_table_scan_next(C4Table *tbl, ScanCursor *cur);

#endif  /* SCAN_CURSOR_H */
