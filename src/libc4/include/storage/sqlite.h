#ifndef C4_SQLITE_H
#define C4_SQLITE_H

#include <sqlite3.h>

typedef struct SQLiteState
{
    C4Runtime *c4;
    sqlite3 *db;
    bool xact_in_progress;
} SQLiteState;

SQLiteState *sqlite_init(C4Runtime *c4);
void sqlite_begin_xact(SQLiteState *sql);
void sqlite_commit_xact(SQLiteState *sql);
void sqlite_exec_sql(SQLiteState *sql, const char *stmt);
sqlite3_stmt *sqlite_pstmt_make(SQLiteState *sql, const char *stmt,
                                int stmt_len, apr_pool_t *pool);

#endif  /* C4_SQLITE_H */
