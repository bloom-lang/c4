#include "c4-internal.h"
#include "storage/mem_table.h"
#include "storage/sqlite_table.h"
#include "storage/table.h"
#include "types/tuple.h"

static apr_status_t abstract_table_cleanup(void *data);

AbstractTable *
table_make(TableDef *def, C4Instance *c4, apr_pool_t *pool)
{
    AbstractTable *tbl;

    switch (def->storage)
    {
        case AST_STORAGE_MEMORY:
            tbl = (AbstractTable *) mem_table_make(def, c4, pool);
            break;

        case AST_STORAGE_SQLITE:
            tbl = (AbstractTable *) sqlite_table_make(def, c4, pool);
            break;

        default:
            ERROR("Unrecognized storage kind: %d", (int) def->storage);
    }

    return tbl;
}

AbstractTable *
table_make_super(apr_size_t sz, TableDef *def,
                 C4Instance *c4, table_insert_f insert_f,
                 table_cleanup_f cleanup_f,
                 table_scan_first_f scan_first_f,
                 table_scan_next_f scan_next_f,
                 apr_pool_t *pool)
{
    AbstractTable *tbl;

    tbl = apr_pcalloc(pool, sz);
    tbl->pool = pool;
    tbl->c4 = c4;
    tbl->def = def;
    tbl->insert = insert_f;
    tbl->cleanup = cleanup_f;
    tbl->scan_first = scan_first_f;
    tbl->scan_next = scan_next_f;

    apr_pool_cleanup_register(pool, tbl, abstract_table_cleanup,
                              apr_pool_cleanup_null);

    /* Caller initializes subtype-specific fields */
    return tbl;
}

static apr_status_t
abstract_table_cleanup(void *data)
{
    AbstractTable *tbl = (AbstractTable *) data;

    tbl->cleanup(tbl);
    return APR_SUCCESS;
}

