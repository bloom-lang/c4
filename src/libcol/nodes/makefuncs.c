#include "col-internal.h"
#include "nodes/makefuncs.h"
#include "types/schema.h"

AstProgram *
make_program(char *name, List *defines, List *facts,
             List *rules, apr_pool_t *pool)
{
    AstProgram *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_PROGRAM;
    result->name = name;
    result->defines = defines;
    result->facts = facts;
    result->rules = rules;
    return result;
}

AstDefine *
make_define(char *name, List *keys, List *schema, apr_pool_t *pool)
{
    AstDefine *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_DEFINE;
    result->name = name;
    result->keys = keys;
    result->schema = schema;
    return result;
}

AstRule *
make_rule(char *name, bool is_delete, bool is_network,
          AstTableRef *head, List *joins, List *quals, apr_pool_t *pool)
{
    AstRule *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_RULE;
    result->is_delete = is_delete;
    result->is_network = is_network;
    result->name = name;
    result->head = head;
    result->joins = joins;
    result->quals = quals;
    return result;
}

AstFact *
make_fact(AstTableRef *head, apr_pool_t *pool)
{
    AstFact *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_FACT;
    result->head = head;
    return result;
}

AstTableRef *
make_table_ref(char *name, List *cols, apr_pool_t *pool)
{
    AstTableRef *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_TABLE_REF;
    result->name = name;
    result->cols = cols;
    return result;
}

AstColumnRef *
make_column_ref(bool has_loc_spec, ColNode *expr, apr_pool_t *pool)
{
    AstColumnRef *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_COLUMN_REF;
    result->has_loc_spec = has_loc_spec;
    result->expr = expr;
    return result;
}

AstJoinClause *
make_join_clause(AstTableRef *ref, bool not, AstHashVariant hash_v,
                 apr_pool_t *pool)
{
    AstJoinClause *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_JOIN_CLAUSE;
    result->ref = ref;
    result->not = not;
    result->hash_variant = hash_v;
    return result;
}

AstOpExpr *
make_op_expr(ColNode *lhs, ColNode *rhs, AstOperKind op_kind, apr_pool_t *pool)
{
    AstOpExpr *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_OP_EXPR;
    result->lhs = lhs;
    result->rhs = rhs;
    result->op_kind = op_kind;
    return result;
}

AstVarExpr *
make_var_expr(char *name, DataType type, apr_pool_t *pool)
{
    AstVarExpr *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_VAR_EXPR;
    result->name = name;
    result->type = type;
    return result;
}

AstConstExpr *
make_const_expr(AstConstKind c_kind, char *value, apr_pool_t *pool)
{
    AstConstExpr *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_CONST_EXPR;
    result->const_kind = c_kind;
    result->value = value;
    return result;
}

AstQualifier *
make_qualifier(ColNode *expr, apr_pool_t *pool)
{
    AstQualifier *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_QUALIFIER;
    result->expr = expr;
    return result;
}
