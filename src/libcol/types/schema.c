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

bool
schema_equal(Schema *s1, Schema *s2)
{
    int i;

    if (s1->len != s2->len)
        return false;

    for (i = 0; i < s1->len; i++)
    {
        if (schema_get_type(s1, i) != schema_get_type(s2, i))
            return false;
    }

    return true;
}
