#include "col-internal.h"
#include "parser/makefuncs.h"
#include "types/schema.h"

AstTableRef *
make_table_ref(char *name, List *cols, apr_pool_t *pool)
{
    AstTableRef *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_TABLE_REF;
    result->name = name;
    result->cols = cols;
    return result;
}

AstOpExpr *
make_op_expr(AstNode *lhs, AstNode *rhs, AstOperKind op_kind, apr_pool_t *pool)
{
    AstOpExpr *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_OP_EXPR;
    result->lhs = lhs;
    result->rhs = rhs;
    result->op_kind = op_kind;
    return result;
}

AstColumnRef *
make_column_ref(bool has_loc_spec, AstNode *expr, apr_pool_t *pool)
{
    AstColumnRef *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_COLUMN_REF;
    result->has_loc_spec = has_loc_spec;
    result->expr = expr;
    return result;
}

AstRule *
make_rule(char *name, bool is_delete, AstTableRef *head, apr_pool_t *pool)
{
    AstRule *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_RULE;
    result->name = name;
    result->is_delete = is_delete;
    result->head = head;
    result->is_network = false;         /* Should be filled-in by caller */
    return result;
}

AstVarExpr *
make_var_expr(char *name, apr_pool_t *pool)
{
    AstVarExpr *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_VAR_EXPR;
    result->name = name;
    result->type = TYPE_INVALID;        /* Should be filled-in by caller */
    return result;
}

AstPredicate *
make_predicate(AstNode *expr, apr_pool_t *pool)
{
    AstPredicate *result = apr_pcalloc(pool, sizeof(*result));
    result->node.kind = AST_PREDICATE;
    result->expr = expr;
    return result;
}
