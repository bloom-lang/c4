#ifndef MAKEFUNCS_H
#define MAKEFUNCS_H

#include "parser/ast.h"
#include "planner/planner.h"

AstProgram *make_program(const char *name, List *defines, List *facts,
                         List *rules, apr_pool_t *p);
AstDefine *make_define(const char *name, List *keys, List *schema,
                       apr_pool_t *p);
AstSchemaElt *make_schema_elt(const char *type_name, bool is_loc_spec,
                              apr_pool_t *p);
AstRule *make_rule(const char *name, bool is_delete, bool is_network,
                   AstTableRef *head, List *joins, List *quals,
                   apr_pool_t *p);
AstFact *make_fact(AstTableRef *head, apr_pool_t *p);
AstTableRef *make_table_ref(const char *name, List *cols, apr_pool_t *p);
AstColumnRef *make_column_ref(ColNode *expr, apr_pool_t *p);
AstJoinClause *make_join_clause(AstTableRef *ref, bool not,
                                AstHashVariant hash_v, apr_pool_t *p);
AstQualifier *make_qualifier(ColNode *expr, apr_pool_t *p);
AstOpExpr *make_op_expr(ColNode *lhs, ColNode *rhs,
                        AstOperKind op_kind, apr_pool_t *p);
AstVarExpr *make_var_expr(const char *name, DataType type, apr_pool_t *p);
AstConstExpr *make_const_expr(AstConstKind c_kind, const char *value, apr_pool_t *p);

FilterPlan *make_filter_plan(List *quals, const char *tbl_name, apr_pool_t *p);
InsertPlan *make_insert_plan(AstTableRef *head, bool is_network, apr_pool_t *p);
ScanPlan *make_scan_plan(List *quals, const char *tbl_name, apr_pool_t *p);

#endif  /* MAKEFUNCS_H */
