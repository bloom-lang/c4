#include "col-internal.h"
#include "types/schema.h"

Schema *
schema_make(int len, DataType *type, apr_pool_t *pool)
{
    Schema *schema;

    schema = apr_palloc(pool, sizeof(*schema) + (len * sizeof(DataType)));
    memset(schema, 0, sizeof(*schema));
    schema->len = len;
    memcpy(schema_get_data(schema), type, len * sizeof(DataType));

    return schema;
}
