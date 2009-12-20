#include "col-internal.h"
#include "operator/scan.h"
#include "operator/scancursor.h"
#include "types/sqlite_table.h"

static apr_status_t sqlite_table_cleanup(void *data);

ColSQLiteTable *
sqlite_table_make(ColTable *ctbl)
{
    StrBuf *stmt;
    StrBuf *pkeys;
    sqlite3_stmt *create_stmt;
    int i;
    int res;

    ctbl->sql_table = apr_pcalloc(ctbl->pool, sizeof(*ctbl->sql_table));
    apr_pool_cleanup_register(ctbl->pool, ctbl->sql_table, sqlite_table_cleanup,
                              apr_pool_cleanup_null);

    stmt = sbuf_make(ctbl->pool);
    pkeys = sbuf_make(ctbl->pool);

    sbuf_appendf(stmt, "CREATE TABLE %s (", ctbl->def->name);
    for (i = 0; i < ctbl->def->schema->len; i++)
    {
        if (i != 0)
        {
            sbuf_append_char(stmt, ',');
            sbuf_append_char(pkeys, ',');
        }

        switch (ctbl->def->schema->types[i])
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
                ERROR("Unexpected data type: %uc", ctbl->def->schema->types[i]);
        }
    }
    sbuf_append_char(pkeys, '\0');
    sbuf_appendf(stmt, ", PRIMARY KEY (%s));", pkeys->data);
    sbuf_append_char(stmt, '\0');
    /* printf(stmt); */

    if ((res = sqlite3_prepare_v2(ctbl->col->sql_db,
                                  stmt->data,
                                  -1,
                                  &create_stmt,  /* OUT: Statement handle */
                                  NULL)))
        ERROR("SQLite prepare failure %d: %s", res, stmt->data);

    res = sqlite3_step(create_stmt);
    if (res != SQLITE_DONE)
        ERROR("SQLite create table failure: %s", stmt->data);
    res = sqlite3_finalize(create_stmt);
    if (res != SQLITE_OK)
        FAIL_SQLITE(ctbl->col);

    return ctbl->sql_table;
}

/*
 * Close the DB connection. Normal table unpins tuples here. What do we do?
 */
static apr_status_t
sqlite_table_cleanup(void *data)
{
    ColTable *ctbl = (ColTable *) data;
    char stmt[1024];
    int stmt_len;
    sqlite3_stmt *delete_stmt;
    int res;

    stmt_len = snprintf(stmt, sizeof(stmt), "DROP TABLE %s;", ctbl->def->name);
    if ((res = sqlite3_prepare_v2(ctbl->col->sql_db,
                                  stmt, stmt_len,
                                  &delete_stmt,  /* OUT: Statement handle */
                                  NULL)))
        ERROR("SQLite prepare failure %d: %s", res, stmt);

    res = sqlite3_step(delete_stmt);
    if (res != SQLITE_DONE)
        ERROR("SQLite drop table failure: %s", stmt);

    if (sqlite3_finalize(delete_stmt) != SQLITE_OK)
        FAIL_SQLITE(ctbl->col);

    if (ctbl->sql_table->insert_stmt != NULL)
    {
        if (sqlite3_finalize(ctbl->sql_table->insert_stmt) != SQLITE_OK)
            FAIL_SQLITE(ctbl->col);
    }

    if (ctbl->sql_table->scan_stmt != NULL)
    {
        if (sqlite3_finalize(ctbl->sql_table->scan_stmt) != SQLITE_OK)
            FAIL_SQLITE(ctbl->col);
    }

    return APR_SUCCESS;
}

/*
 * Insert the tuple into this table. Returns "true" if the tuple was added;
 * returns false if the insert was a no-op because the tuple is already
 * contained by this table.
 */
bool
sqlite_table_insert(ColTable *ctbl, Tuple *t)
{
    /* take prepared SQL statement, use Schema to walk the tuple for insert constants. */
    ColSQLiteTable *sql_table = ctbl->sql_table;
    DataType *types = ctbl->def->schema->types;
    int i;
    int res;

    if (sql_table->insert_stmt == NULL)
    {
        /* Prepare an insert statement */
        char *param_str;
        char stmt[1024];
        int stmt_len;

        /* XXX: memory leak */
        param_str = schema_to_sql_param_str(tuple_get_schema(t), ctbl->pool);

        /* XXX: need to escape SQL string */
        stmt_len = snprintf(stmt, sizeof(stmt), "INSERT INTO %s VALUES (%s);",
                            ctbl->def->name, param_str);

        if ((res = sqlite3_prepare_v2(ctbl->col->sql_db,
                                      stmt, stmt_len,
                                      &sql_table->insert_stmt,
                                      NULL)))
            ERROR("SQLite prepare failure %d: %s", res, stmt);
    }
    else
        sqlite3_reset(sql_table->insert_stmt);

    for (i = 0; i < ctbl->def->schema->len; i++)
    {
        Datum val = tuple_get_val(t, i);

        switch (types[i])
        {
            case TYPE_BOOL:
                sqlite3_bind_int(sql_table->insert_stmt, i + 1, val.b);
                break;
            case TYPE_CHAR:
                sqlite3_bind_int(sql_table->insert_stmt, i + 1, val.c);
                break;
            case TYPE_INT2:
                sqlite3_bind_int(sql_table->insert_stmt, i + 1, val.i2);
                break;
            case TYPE_INT4:
                sqlite3_bind_int(sql_table->insert_stmt, i + 1, val.i4);
                break;
            case TYPE_INT8:
                sqlite3_bind_int64(sql_table->insert_stmt, i + 1, val.i8);
                break;
            case TYPE_DOUBLE:
                sqlite3_bind_double(sql_table->insert_stmt, i + 1, val.d8);
                break;
            case TYPE_STRING:
                sqlite3_bind_text(sql_table->insert_stmt, i + 1, val.s->data,
                                  val.s->len, SQLITE_STATIC);
                break;

            case TYPE_INVALID:
                ERROR("Invalid data type: TYPE_INVALID");
            default:
                ERROR("Unexpected data type: %uc", types[i]);
        }
    }

    do {
        res = sqlite3_step(sql_table->insert_stmt);
    } while (res == SQLITE_OK);

    /* We assume that a constraint failure is a PK violation due to a dup */
    if (res == SQLITE_CONSTRAINT)
        return false;
    if (res != SQLITE_DONE)
        FAIL_SQLITE(ctbl->col);

    return true;        /* Not a duplicate */
}

Tuple *
sqlite_table_scan_first(ColTable *ctbl, ScanCursor *cur)
{
    if (cur->sqlite_stmt == NULL)
    {
        char stmt[1024];
        int stmt_len;

        /* XXX: escape table name? */
        stmt_len = snprintf(stmt, sizeof(stmt), "SELECT * FROM %s;",
                            ctbl->def->name);

        sqlite3_prepare_v2(ctbl->col->sql_db,
                           stmt, stmt_len,
                           &cur->sqlite_stmt, /* OUT: Statement handle */
                           NULL);
    }
    else
        sqlite3_reset(cur->sqlite_stmt);

    return sqlite_table_scan_next(ctbl, cur);
}

Tuple *
sqlite_table_scan_next(ColTable *ctbl, ScanCursor *cur)
{
    Schema *schema = ctbl->def->schema;
    Tuple *tuple;
    int res;
    int i;

    res = sqlite3_step(cur->sqlite_stmt);
    if (res == SQLITE_DONE)
        return NULL;
    if (res != SQLITE_ROW)
        FAIL_SQLITE(ctbl->col);

    tuple = tuple_make_empty(schema);

    /* iterate through the columns and construct a Tuple struct */
    for (i = 0; i < schema->len; i++)
    {
        Datum d;

        switch (schema->types[i])
        {
            case TYPE_BOOL:
                d.b = (bool) sqlite3_column_int(cur->sqlite_stmt, i);
                break;
            case TYPE_CHAR:
                d.c = (unsigned char) sqlite3_column_int(cur->sqlite_stmt, i);
                break;
            case TYPE_INT2:
                d.i2 = (apr_int16_t) sqlite3_column_int(cur->sqlite_stmt, i);
                break;
            case TYPE_INT4:
                d.i4 = (apr_int32_t) sqlite3_column_int(cur->sqlite_stmt, i);
                break;
            case TYPE_INT8:
                d.i8 = (apr_int64_t) sqlite3_column_int64(cur->sqlite_stmt, i);
                break;
            case TYPE_DOUBLE:
                d.d8 = (double) sqlite3_column_double(cur->sqlite_stmt, i);
                break;
            case TYPE_STRING:
                d = string_from_str((const char *) sqlite3_column_text(cur->sqlite_stmt, i));
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
