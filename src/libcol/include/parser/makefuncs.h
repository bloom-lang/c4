#ifndef MAKEFUNCS_H
#define MAKEFUNCS_H

#include "parser/ast.h"

AstProgram *make_program(char *name, List *defines, List *facts,
                         List *rules, apr_pool_t *pool);
AstDefine *make_define(char *name, List *keys, List *schema,
                       apr_pool_t *pool);
AstRule *make_rule(char *name, bool is_delete, bool is_network,
                   AstTableRef *head, List *body, apr_pool_t *pool);
AstFact *make_fact(AstTableRef *head, apr_pool_t *pool);
AstTableRef *make_table_ref(char *name, List *cols, apr_pool_t *pool);
AstJoinClause *make_join_clause(AstTableRef *ref, bool not,
                                AstHashVariant hash_v, apr_pool_t *pool);
AstOpExpr *make_op_expr(AstNode *lhs, AstNode *rhs,
                        AstOperKind op_kind, apr_pool_t *pool);
AstColumnRef *make_column_ref(bool has_loc_spec, AstNode *expr,
                              apr_pool_t *pool);
AstVarExpr *make_var_expr(char *name, DataType type, apr_pool_t *pool);
AstQualifier *make_qualifier(AstNode *expr, apr_pool_t *pool);

#endif  /* MAKEFUNCS_H */
