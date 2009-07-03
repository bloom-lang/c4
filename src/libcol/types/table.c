#include <apr_strings.h>

#include "col-internal.h"
#include "types/table.h"
#include "types/tuple.h"

static apr_status_t table_cleanup(void *data);
static int table_cmp_tuple(const void *k1, const void *k2, apr_size_t klen);

ColTable *
table_make(const char *name, apr_pool_t *pool)
{
    ColTable *tbl;

    tbl = apr_pcalloc(pool, sizeof(*tbl));
    tbl->pool = pool;
    tbl->name = apr_pstrdup(pool, name);
    tbl->tuples = col_hash_make_custom(pool,
                                       tuple_hash,
                                       table_cmp_tuple);

    apr_pool_cleanup_register(pool, tbl, table_cleanup,
                              apr_pool_cleanup_null);

    return tbl;
}

/*
 * Unpin the tuples contained in this table.
 */
static apr_status_t
table_cleanup(void *data)
{
    ColTable *tbl = (ColTable *) data;
    col_hash_index_t *hi;

    for (hi = col_hash_first(tbl->pool, tbl->tuples);
         hi != NULL; hi = col_hash_next(hi))
    {
        Tuple *t;

        col_hash_this(hi, (const void **) &t, NULL, NULL);
        tuple_unpin(t);
    }

    return APR_SUCCESS;
}

static int
table_cmp_tuple(const void *k1, const void *k2, apr_size_t klen)
{
    Tuple *t1 = (Tuple *) k1;
    Tuple *t2 = (Tuple *) k2;

    return 0;
}

/*
 * Add the specified tuple to this table. Returns "true" if the tuple was
 * added; returns false if the insert was a no-op because the tuple is
 * already contained by this table.
 */
bool
table_insert(ColTable *tbl, Tuple *t)
{
    Tuple *val;

    val = col_hash_set_if_new(tbl->tuples, t, sizeof(t), t);
    tuple_pin(val);

    return (bool) (t == val);
}
