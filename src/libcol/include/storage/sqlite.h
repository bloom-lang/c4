#ifndef COL_SQLITE_H
#define COL_SQLITE_H

typedef struct SQLiteState
{
    ColInstance *col;
    sqlite3 *db;
    bool xact_in_progress;
} SQLiteState;

SQLiteState *sqlite_init(ColInstance *col);
void sqlite_begin_xact(SQLiteState *sql);
void sqlite_commit_xact(SQLiteState *sql);

#endif  /* COL_SQLITE_H */
