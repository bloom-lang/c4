#include "c4-internal.h"
#include "storage/sqlite.h"

static apr_status_t sqlite_cleanup(void *data);
static apr_status_t sqlite_pstmt_cleanup(void *data);

SQLiteState *
sqlite_init(C4Runtime *c4)
{
    SQLiteState *sql;
    char *db_fname;
    int res;

    sql = apr_palloc(c4->pool, sizeof(*sql));
    sql->c4 = c4;
    sql->xact_in_progress = false;

    db_fname = apr_pstrcat(c4->tmp_pool, c4->base_dir, "/", "sqlite.db", NULL);
    if ((res = sqlite3_open(db_fname, &sql->db)) != 0)
        ERROR("sqlite3_open failed: %d", res);

    apr_pool_cleanup_register(c4->pool, sql, sqlite_cleanup,
                              apr_pool_cleanup_null);

    return sql;
}

static apr_status_t
sqlite_cleanup(void *data)
{
    SQLiteState *sql = (SQLiteState *) data;

    if (sql->xact_in_progress)
        FAIL();

    (void) sqlite3_close(sql->db);

    return APR_SUCCESS;
}

void
sqlite_begin_xact(SQLiteState *sql)
{
    if (sql->xact_in_progress)
        FAIL();

    sqlite_exec_sql(sql, "BEGIN;");
    sql->xact_in_progress = true;
}

void
sqlite_commit_xact(SQLiteState *sql)
{
    if (!sql->xact_in_progress)
        FAIL();

    sqlite_exec_sql(sql, "COMMIT;");
    sql->xact_in_progress = false;
}

void
sqlite_exec_sql(SQLiteState *sql, const char *stmt)
{
    if (sqlite3_exec(sql->db, stmt, NULL, NULL, NULL) != SQLITE_OK)
        FAIL_SQLITE(sql->c4);
}

/*
 * Create a new SQLite3 prepared statement, and register a cleanup
 * function in the given pool. stmt_len can be -1 to use strlen(stmt).
 */
sqlite3_stmt *
sqlite_pstmt_make(SQLiteState *sql, const char *stmt, int stmt_len,
                  apr_pool_t *pool)
{
    sqlite3_stmt *pstmt;

    if (sqlite3_prepare_v2(sql->db, stmt, stmt_len, &pstmt, NULL) != SQLITE_OK)
        FAIL_SQLITE(sql->c4);

    apr_pool_cleanup_register(pool, pstmt, sqlite_pstmt_cleanup,
                              apr_pool_cleanup_null);

    return pstmt;
}

static apr_status_t
sqlite_pstmt_cleanup(void *data)
{
    sqlite3_stmt *stmt = (sqlite3_stmt *) data;

    if (sqlite3_finalize(stmt) != SQLITE_OK)
        FAIL();         /* XXX: better error recovery */

    return APR_SUCCESS;
}

