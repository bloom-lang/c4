#ifndef SCHEMA_H
#define SCHEMA_H

/*
 * This is effectively a set of type IDs. A more sophisticated type ID
 * system is probably a natural next step.
 */
typedef unsigned char DataType;

#define TYPE_INVALID 0
#define TYPE_BOOL    1
#define TYPE_CHAR    2
#define TYPE_DOUBLE  3
#define TYPE_INT2    4
#define TYPE_INT4    5
#define TYPE_INT8    6
#define TYPE_STRING  7

typedef struct Schema
{
    int len;
    /* XXX: Unnecessary padding on a 32-bit machine */
    char c[1];
} Schema;

#define schema_get_data(s)      (((Schema *) s) + 1)
#define schema_get_type(s, i)   (schema_get_data(s)[(i)])

Schema *schema_make(int len, DataType *type, apr_pool_t *pool);

#endif  /* SCHEMA_H */
