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
is_valid_type_name(const char *type_name)
{
    return (bool) (lookup_type_name(type_name) != TYPE_INVALID);
}

DataType
lookup_type_name(const char *type_name)
{
    if (strcmp(type_name, "bool") == 0)
        return TYPE_BOOL;
    if (strcmp(type_name, "char") == 0)
        return TYPE_CHAR;
    if (strcmp(type_name, "double") == 0)
        return TYPE_DOUBLE;
    if (strcmp(type_name, "int") == 0)
        return TYPE_INT4;
    if (strcmp(type_name, "int2") == 0)
        return TYPE_INT2;
    if (strcmp(type_name, "int4") == 0)
        return TYPE_INT4;
    if (strcmp(type_name, "int8") == 0)
        return TYPE_INT8;
    if (strcmp(type_name, "string") == 0)
        return TYPE_STRING;

    return TYPE_INVALID;
}
