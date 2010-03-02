#ifndef ANALYZE_H
#define ANALYZE_H

#include <apr_hash.h>

#include "parser/ast.h"
#include "types/schema.h"
#include "util/list.h"

void analyze_ast(AstProgram *program, apr_pool_t *pool, C4Runtime *c4);

/* Utility functions */
DataType expr_get_type(C4Node *node);
bool is_simple_equality(AstQualifier *qual, AstVarExpr **lhs, AstVarExpr **rhs);

List *get_eq_list(const char *var_name, bool make_new, apr_hash_t *eq_tbl,
                  apr_pool_t *p);
void add_var_eq(char *v1, char *v2, apr_hash_t *eq_tbl, apr_pool_t *p);

#endif  /* ANALYZE_H */
