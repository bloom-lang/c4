#ifndef SCHEMA_H
#define SCHEMA_H

#include "types/datum.h"
#include "util/error.h"
#include "util/list.h"
#include "util/tuple_pool.h"

typedef struct Schema
{
    int len;
    DataType *types;
    datum_hash_func *hash_funcs;
    datum_eq_func *eq_funcs;
    datum_bin_in_func *bin_in_funcs;
    datum_text_in_func *text_in_funcs;
    datum_bin_out_func *bin_out_funcs;
    datum_text_out_func *text_out_funcs;
    /* Used to manage tuple allocations */
    TuplePool *tuple_pool;
} Schema;

struct ExprState;

Schema *schema_make(int len, DataType *types, apr_pool_t *pool);
Schema *schema_make_from_ast(List *ast_schema, apr_pool_t *pool);
Schema *schema_make_from_exprs(int len, struct ExprState **expr_ary, apr_pool_t *pool);
bool schema_equal(Schema *s1, Schema *s2);
apr_size_t schema_get_tuple_size(Schema *schema);
char *schema_to_sql_param_str(Schema *schema, apr_pool_t *pool);

static inline DataType
schema_get_type(Schema *s, int idx)
{
    ASSERT(idx >= 0);
    ASSERT(idx < s->len);

    return s->types[idx];
}

#endif  /* SCHEMA_H */
