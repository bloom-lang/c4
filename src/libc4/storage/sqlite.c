#include "c4-internal.h"
#include "storage/sqlite.h"

static apr_status_t sqlite_cleanup(void *data);

SQLiteState *
sqlite_init(C4Instance *c4)
{
    SQLiteState *sql;
    char *db_fname;
    int res;

    sql = apr_palloc(c4->pool, sizeof(*sql));
    sql->c4 = c4;
    sql->xact_in_progress = false;

    db_fname = apr_pstrcat(c4->pool, c4->base_dir, "/", "sqlite.db", NULL);
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
