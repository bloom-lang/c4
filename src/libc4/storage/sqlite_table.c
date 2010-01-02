#include "c4-internal.h"
#include "operator/scan.h"
#include "operator/scancursor.h"
#include "storage/sqlite.h"
#include "storage/sqlite_table.h"

static void sqlite_table_create_sql(SQLiteTable *tbl);
static void sqlite_table_cleanup(AbstractTable *a_tbl);
static bool sqlite_table_insert(AbstractTable *a_tbl, Tuple *t);
static ScanCursor *sqlite_table_scan_make(AbstractTable *a_tbl, apr_pool_t *pool);
static void sqlite_table_scan_reset(AbstractTable *a_tbl, ScanCursor *scan);
static Tuple *sqlite_table_scan_next(AbstractTable *a_tbl, ScanCursor *cur);

SQLiteTable *
sqlite_table_make(TableDef *def, C4Instance *c4, apr_pool_t *pool)
{
    SQLiteTable *tbl;

    tbl = (SQLiteTable *) table_make_super(sizeof(*tbl), def, c4,
                                           sqlite_table_insert,
                                           sqlite_table_cleanup,
                                           sqlite_table_scan_make,
                                           sqlite_table_scan_reset,
                                           sqlite_table_scan_next,
                                           pool);

    sqlite_table_create_sql(tbl);

    return tbl;
}

static void
sqlite_table_create_sql(SQLiteTable *tbl)
{
    TableDef *tbl_def = tbl->table.def;
    Schema *schema = tbl_def->schema;
    StrBuf *stmt;
    StrBuf *pkeys;
    int i;

    stmt = sbuf_make(tbl->table.pool);
    pkeys = sbuf_make(tbl->table.pool);

    sbuf_appendf(stmt, "CREATE TABLE %s (", tbl_def->name);
    for (i = 0; i < schema->len; i++)
    {
        if (i != 0)
        {
            sbuf_append_char(stmt, ',');
            sbuf_append_char(pkeys, ',');
        }

        switch (schema->types[i])
        {
            case TYPE_BOOL:
            case TYPE_INT2:
            case TYPE_CHAR:
            case TYPE_INT4:
            case TYPE_INT8:
                sbuf_appendf(stmt, "c%d integer", i);
                sbuf_appendf(pkeys, "c%d", i);
                break;

            case TYPE_DOUBLE:
                sbuf_appendf(stmt, "c%d real", i);
                sbuf_appendf(pkeys, "c%d", i);
                break;

            case TYPE_STRING:
                sbuf_appendf(stmt, "c%d text", i);
                sbuf_appendf(pkeys, "c%d", i);
                break;

            case TYPE_INVALID:
                ERROR("Invalid data type: TYPE_INVALID");

            default:
                ERROR("Unexpected data type: %uc", schema->types[i]);
        }
    }
    sbuf_append_char(pkeys, '\0');
    sbuf_appendf(stmt, ", PRIMARY KEY (%s));", pkeys->data);
    sbuf_append_char(stmt, '\0');

    sqlite_exec_sql(tbl->table.c4->sql, stmt->data);
}

/*
 * Close the DB connection. MemTable unpins tuples here. What do we do?
 */
static void
sqlite_table_cleanup(AbstractTable *a_tbl)
{
    char stmt[1024];

    snprintf(stmt, sizeof(stmt), "DROP TABLE %s;", a_tbl->def->name);
    sqlite_exec_sql(a_tbl->c4->sql, stmt);
}

/*
 * Insert the tuple into this table. Returns "true" if the tuple was added;
 * returns false if the insert was a no-op because the tuple is already
 * contained by this table.
 */
static bool
sqlite_table_insert(AbstractTable *a_tbl, Tuple *t)
{
    /* take prepared SQL statement, use Schema to walk the tuple for insert constants. */
    SQLiteTable *tbl = (SQLiteTable *) a_tbl;
    TableDef *tbl_def = a_tbl->def;
    DataType *types = tbl_def->schema->types;
    SQLiteState *sql = a_tbl->c4->sql;
    int i;
    int res;

    if (tbl->insert_stmt == NULL)
    {
        /* Prepare an insert statement */
        char *param_str;
        char stmt[1024];
        int stmt_len;

        /* XXX: memory leak */
        param_str = schema_to_sql_param_str(tuple_get_schema(t), a_tbl->pool);

        /* XXX: need to escape SQL string */
        stmt_len = snprintf(stmt, sizeof(stmt), "INSERT INTO %s VALUES (%s);",
                            tbl_def->name, param_str);
        tbl->insert_stmt = sqlite_pstmt_make(sql, stmt, stmt_len, a_tbl->pool);
    }
    else
        (void) sqlite3_reset(tbl->insert_stmt);

    for (i = 0; i < tbl_def->schema->len; i++)
    {
        Datum val = tuple_get_val(t, i);

        switch (types[i])
        {
            case TYPE_BOOL:
                sqlite3_bind_int(tbl->insert_stmt, i + 1, val.b);
                break;
            case TYPE_CHAR:
                sqlite3_bind_int(tbl->insert_stmt, i + 1, val.c);
                break;
            case TYPE_INT2:
                sqlite3_bind_int(tbl->insert_stmt, i + 1, val.i2);
                break;
            case TYPE_INT4:
                sqlite3_bind_int(tbl->insert_stmt, i + 1, val.i4);
                break;
            case TYPE_INT8:
                sqlite3_bind_int64(tbl->insert_stmt, i + 1, val.i8);
                break;
            case TYPE_DOUBLE:
                sqlite3_bind_double(tbl->insert_stmt, i + 1, val.d8);
                break;
            case TYPE_STRING:
                sqlite3_bind_text(tbl->insert_stmt, i + 1, val.s->data,
                                  val.s->len, SQLITE_STATIC);
                break;

            case TYPE_INVALID:
                ERROR("Invalid data type: TYPE_INVALID");
            default:
                ERROR("Unexpected data type: %uc", types[i]);
        }
    }

    /* If we're not inside an xact block, start one */
    if (!sql->xact_in_progress)
        sqlite_begin_xact(sql);

    do {
        res = sqlite3_step(tbl->insert_stmt);
    } while (res == SQLITE_OK);

    /* We assume that a constraint failure is a PK violation due to a dup */
    if (res == SQLITE_CONSTRAINT)
        return false;
    if (res != SQLITE_DONE)
        FAIL_SQLITE(tbl->table.c4);

    return true;        /* Not a duplicate */
}

static ScanCursor *
sqlite_table_scan_make(AbstractTable *a_tbl, apr_pool_t *pool)
{
    ScanCursor *scan;
    char stmt[1024];
    int stmt_len;

    scan = apr_pcalloc(pool, sizeof(*scan));
    scan->pool = pool;

    /* XXX: escape table name? */
    stmt_len = snprintf(stmt, sizeof(stmt), "SELECT * FROM %s;",
                        a_tbl->def->name);
    scan->sqlite_stmt = sqlite_pstmt_make(a_tbl->c4->sql, stmt, stmt_len, pool);

    return scan;
}

static void
sqlite_table_scan_reset(AbstractTable *a_tbl, ScanCursor *scan)
{
    (void) sqlite3_reset(scan->sqlite_stmt);
}

/*
 * Construct a C4 tuple from the next row of the SQLite result set.
 *
 * XXX: when we advance to a subsequent SQLite row, the storage for the previous
 * row is released; we currently get this wrong, because we don't copy when
 * building C4 datums.
 */
static Tuple *
sqlite_table_scan_next(AbstractTable *a_tbl, ScanCursor *scan)
{
    Schema *schema = a_tbl->def->schema;
    Tuple *tuple;
    int res;
    int i;

    res = sqlite3_step(scan->sqlite_stmt);
    if (res == SQLITE_DONE)
        return NULL;
    if (res != SQLITE_ROW)
        FAIL_SQLITE(a_tbl->c4);

    tuple = tuple_make_empty(schema);

    for (i = 0; i < schema->len; i++)
    {
        Datum d;

        switch (schema->types[i])
        {
            case TYPE_BOOL:
                d.b = (bool) sqlite3_column_int(scan->sqlite_stmt, i);
                break;
            case TYPE_CHAR:
                d.c = (unsigned char) sqlite3_column_int(scan->sqlite_stmt, i);
                break;
            case TYPE_INT2:
                d.i2 = (apr_int16_t) sqlite3_column_int(scan->sqlite_stmt, i);
                break;
            case TYPE_INT4:
                d.i4 = (apr_int32_t) sqlite3_column_int(scan->sqlite_stmt, i);
                break;
            case TYPE_INT8:
                d.i8 = (apr_int64_t) sqlite3_column_int64(scan->sqlite_stmt, i);
                break;
            case TYPE_DOUBLE:
                d.d8 = (double) sqlite3_column_double(scan->sqlite_stmt, i);
                break;
            case TYPE_STRING:
                d = string_from_str((const char *) sqlite3_column_text(scan->sqlite_stmt, i));
                break;

            case TYPE_INVALID:
                ERROR("Invalid data type: TYPE_INVALID");
            default:
                ERROR("Unexpected data type: %uc", schema->types[i]);
        }

        tuple->vals[i] = d;
    }

    return tuple;
}
