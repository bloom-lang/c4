#include "c4-internal.h"
#include "operator/scancursor.h"
#include "storage/mem_table.h"

static void mem_table_cleanup(AbstractTable *a_tbl);
static bool mem_table_insert(AbstractTable *a_tbl, Tuple *t);
static Tuple *mem_table_scan_first(AbstractTable *a_tbl, ScanCursor *cur);
static Tuple *mem_table_scan_next(AbstractTable *a_tbl, ScanCursor *cur);
static int mem_table_cmp_tuple(const void *k1, const void *k2, apr_size_t klen);
static unsigned int mem_table_hash_tuple(const char *key, apr_ssize_t *klen);

MemTable *
mem_table_make(TableDef *def, C4Instance *c4, apr_pool_t *pool)
{
    MemTable *tbl;

    tbl = (MemTable *) table_make_super(sizeof(*tbl), def, c4,
                                        mem_table_insert,
                                        mem_table_cleanup,
                                        mem_table_scan_first,
                                        mem_table_scan_next,
                                        pool);
    tbl->tuples = c4_hash_make_custom(pool,
                                      mem_table_hash_tuple,
                                      mem_table_cmp_tuple);

    return tbl;
}

/*
 * Unpin the tuples contained in this table.
 */
static void
mem_table_cleanup(AbstractTable *a_tbl)
{
    MemTable *tbl = (MemTable *) a_tbl;
    c4_hash_index_t *hi;

    for (hi = c4_hash_first(a_tbl->pool, tbl->tuples);
         hi != NULL; hi = c4_hash_next(hi))
    {
        Tuple *t;

        c4_hash_this(hi, (const void **) &t, NULL, NULL);
        tuple_unpin(t);
    }
}

static bool
mem_table_insert(AbstractTable *a_tbl, Tuple *t)
{
    MemTable *tbl = (MemTable *) a_tbl;
    Tuple *val;

    val = c4_hash_set_if_new(tbl->tuples, t, sizeof(t), t);
    tuple_pin(val);
    return (bool) (t == val);
}

static int
mem_table_cmp_tuple(const void *k1, const void *k2, apr_size_t klen)
{
    Tuple *t1 = (Tuple *) k1;
    Tuple *t2 = (Tuple *) k2;

    ASSERT(klen == sizeof(Tuple *));

    if (tuple_equal(t1, t2))
        return 0;
    else
        return 1;
}

static unsigned int
mem_table_hash_tuple(const char *key, apr_ssize_t *klen)
{
    Tuple *t = (Tuple *) key;

    ASSERT(*klen == sizeof(Tuple *));
    return tuple_hash(t);
}

static Tuple *
mem_table_scan_first(AbstractTable *a_tbl, ScanCursor *cur)
{
    MemTable *tbl = (MemTable *) a_tbl;
	Tuple *ret_tuple;

    /* XXX: Shouldn't share iterator among multiple scans */
    cur->hi = c4_hash_first(NULL, tbl->tuples);

    c4_hash_this(cur->hi, (const void **) &ret_tuple, NULL, NULL);
    return ret_tuple;
}

static Tuple *
mem_table_scan_next(AbstractTable *a_tbl, ScanCursor *cur)
{
	Tuple *ret_tuple;

    cur->hi = c4_hash_next(cur->hi);
    if (cur->hi == NULL)
        return NULL;

    c4_hash_this(cur->hi, (const void **) &ret_tuple, NULL, NULL);
    return ret_tuple;
}
