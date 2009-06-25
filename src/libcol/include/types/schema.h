#ifndef SCHEMA_H
#define SCHEMA_H

/*
 * This effectively a set of type IDs. A more sophisticated type ID system
 * is probably a natural next step.
 */
typedef enum DataType
{
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_DOUBLE,
    TYPE_INT2,
    TYPE_INT4,
    TYPE_INT8,
    TYPE_STRING
} DataType;

typedef struct Schema
{
    int len;
    /* XXX: Unnecessary padding on a 32-bit machine */
    char c[1];
} Schema;

#define schema_get_data(s)      (((Schema *) s) + 1)
#define schema_get_type(s, i)   (get_schema_data(s)[(i)])

Schema *schema_make(int len, DataType *type, apr_pool_t *pool);

#endif  /* SCHEMA_H */
