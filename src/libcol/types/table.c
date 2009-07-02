#include <apr_strings.h>

#include "col-internal.h"
#include "types/table.h"

ColTable *
table_make(const char *name, apr_pool_t *pool)
{
    ColTable *tbl;

    tbl = apr_pcalloc(pool, sizeof(*tbl));
    tbl->pool = pool;
    tbl->name = apr_pstrdup(pool, name);

    return tbl;
}

void
table_insert(ColTable *tbl, Tuple *t)
{
    tuple_pin(t);
}
