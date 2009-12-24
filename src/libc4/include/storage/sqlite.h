#ifndef C4_SQLITE_H
#define C4_SQLITE_H

typedef struct SQLiteState
{
    C4Instance *c4;
    sqlite3 *db;
    bool xact_in_progress;
} SQLiteState;

SQLiteState *sqlite_init(C4Instance *c4);
void sqlite_begin_xact(SQLiteState *sql);
void sqlite_commit_xact(SQLiteState *sql);
void sqlite_exec_sql(SQLiteState *sql, const char *stmt);

#endif  /* C4_SQLITE_H */
