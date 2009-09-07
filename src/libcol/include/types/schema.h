#ifndef SCHEMA_H
#define SCHEMA_H

#include "util/list.h"

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
    DataType *types;
} Schema;

struct ExprState;

Schema *schema_make(int len, DataType *types, apr_pool_t *pool);
Schema *schema_make_from_ast(List *ast_schema, apr_pool_t *pool);
Schema *schema_make_from_exprs(int len, struct ExprState **expr_ary, apr_pool_t *pool);
DataType schema_get_type(Schema *s, int idx);
bool schema_equal(Schema *s1, Schema *s2);

#endif  /* SCHEMA_H */
