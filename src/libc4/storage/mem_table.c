#include "c4-internal.h"
#include "operator/scancursor.h"
#include "storage/mem_table.h"

/*
 * Unpin the tuples contained in this table.
 */
static void
mem_table_cleanup(AbstractTable *a_tbl)
{
    MemTable *tbl = (MemTable *) a_tbl;
    rset_index_t *ri;

    ri = rset_iter_make(a_tbl->pool, tbl->tuples);
    while (rset_next(ri))
    {
        Tuple *t;

        t = rset_this(ri);
        tuple_unpin(t, a_tbl->def->schema);
    }
}

static bool
mem_table_insert(AbstractTable *a_tbl, Tuple *t)
{
    MemTable *tbl = (MemTable *) a_tbl;
    bool is_new;

    is_new = rset_add(tbl->tuples, t);
    if (is_new)
        tuple_pin(t);

    return is_new;
}

static bool
mem_table_delete(AbstractTable *a_tbl, Tuple *t)
{
    MemTable *tbl = (MemTable *) a_tbl;
    Tuple *old_t;
    unsigned int new_count;

    old_t = rset_remove(tbl->tuples, t, &new_count);
    if (old_t != NULL && new_count == 0)
    {
        tuple_unpin(old_t, a_tbl->def->schema);
        return true;
    }

    /* Either not found or updated ref count > 0 */
    return false;
}

static ScanCursor *
mem_table_scan_make(AbstractTable *a_tbl, apr_pool_t *pool)
{
    MemTable *tbl = (MemTable *) a_tbl;
    ScanCursor *scan;

    scan = apr_pcalloc(pool, sizeof(*scan));
    scan->pool = pool;
    scan->rset_iter = rset_iter_make(pool, tbl->tuples);

    return scan;
}

static void
mem_table_scan_reset(AbstractTable *a_tbl, ScanCursor *scan)
{
    rset_iter_reset(scan->rset_iter);
}

static Tuple *
mem_table_scan_next(AbstractTable *a_tbl, ScanCursor *cur)
{
    if (!rset_iter_next(cur->rset_iter))
        return NULL;

    return rset_this(cur->rset_iter);
}

MemTable *
mem_table_make(TableDef *def, C4Runtime *c4, apr_pool_t *pool)
{
    MemTable *tbl;

    tbl = (MemTable *) table_make_super(sizeof(*tbl), def, c4,
                                        mem_table_insert,
                                        mem_table_delete,
                                        mem_table_cleanup,
                                        mem_table_scan_make,
                                        mem_table_scan_reset,
                                        mem_table_scan_next,
                                        pool);
    tbl->tuples = rset_make(pool, def->schema,
                            tuple_hash_tbl, tuple_cmp_tbl);

    return tbl;
}
