#include "col-internal.h"
#include "types/schema.h"

Schema *
schema_make(int len, DataType *type, apr_pool_t *pool)
{
    Schema *schema;

    schema = apr_palloc(pool, sizeof(*schema) + (len * sizeof(DataType)));
    schema->len = len;
    memcpy(((char *) schema) + sizeof(Schema), type, len * sizeof(DataType));

    return schema;
}

