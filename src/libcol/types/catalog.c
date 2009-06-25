#include <apr_hash.h>

#include "col-internal.h"
#include "types/catalog.h"

struct ColCatalog
{
    ColInstance *col;
    apr_pool_t *pool;

    apr_hash_t *schema_tbl;
};

ColCatalog *
cat_make(ColInstance *col)
{
    apr_pool_t *pool;
    ColCatalog *cat;

    pool = make_subpool(col->pool);
    cat = apr_pcalloc(pool, sizeof(*cat));
    cat->col = col;
    cat->pool = pool;
    cat->schema_tbl = apr_hash_make(cat->pool);

    return cat;
}

Schema *
cat_get_schema(ColCatalog *cat, const char *name)
{
    return apr_hash_get(cat->schema_tbl, name, APR_HASH_KEY_STRING);
}

void
cat_set_schema(ColCatalog *cat, const char *name, Schema *schema)
{
    if (cat_get_schema(cat, name) != NULL)
        ERROR("duplicate schema definition: %s", name);

    apr_hash_set(cat->schema_tbl, name,
                 APR_HASH_KEY_STRING, schema);
}
