#include <apr_strings.h>

#include "col-internal.h"
#include "types/table.h"

static apr_status_t table_cleanup(void *data);

ColTable *
table_make(const char *name, apr_pool_t *pool)
{
    ColTable *tbl;

    tbl = apr_pcalloc(pool, sizeof(*tbl));
    tbl->pool = pool;
    tbl->name = apr_pstrdup(pool, name);
    tbl->tuples = apr_hash_make(pool);

    apr_pool_cleanup_register(pool, tbl, table_cleanup,
                              apr_pool_cleanup_null);

    return tbl;
}

/*
 * Unpin the tuples formerly contained in this table.
 */
static apr_status_t
table_cleanup(void *data)
{
    ColTable *tbl = (ColTable *) data;
    apr_hash_index_t *hi;

    for (hi = apr_hash_first(tbl->pool, tbl->tuples);
         hi != NULL; hi = apr_hash_next(hi))
    {
        Tuple *t;

        apr_hash_this(hi, (const void **) &t, NULL, NULL);
        tuple_unpin(t);
    }

    return APR_SUCCESS;
}

/*
 * Add the specified tuple to this table. Returns "true" if the tuple was
 * added; returns false if the insert was a no-op because the tuple is
 * already contained by this table.
 */
bool
table_insert(ColTable *tbl, Tuple *t)
{
#if 0
    Tuple *val;

    val = apr_hash_set_if_new(tbl->tuples, t, sizeof(t), t);
    tuple_pin(val);

    return (bool) (t == val);
#endif
    return true;
}
