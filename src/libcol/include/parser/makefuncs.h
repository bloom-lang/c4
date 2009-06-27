#ifndef MAKEFUNCS_H
#define MAKEFUNCS_H

#include "parser/ast.h"

AstTableRef *make_table_ref(char *name, List *cols, apr_pool_t *pool);
AstOpExpr *make_op_expr(AstNode *lhs, AstNode *rhs,
                        AstOperKind op_kind, apr_pool_t *pool);
AstColumnRef *make_column_ref(bool has_loc_spec, AstNode *expr,
                              apr_pool_t *pool);
AstRule *make_rule(char *name, bool is_delete, AstTableRef *head,
                   apr_pool_t *pool);
AstVarExpr *make_var_expr(char *name, apr_pool_t *pool);
AstQualifier *make_qualifier(AstNode *expr, apr_pool_t *pool);

#endif  /* MAKEFUNCS_H */
