#include <apr_strings.h>

#include "col-internal.h"
#include "parser/copyfuncs.h"
#include "parser/makefuncs.h"

static AstProgram *
program_copy(AstProgram *in, apr_pool_t *p)
{
    return make_program(apr_pstrdup(p, in->name),
                        list_copy_deep(in->defines, p),
                        list_copy_deep(in->facts, p),
                        list_copy_deep(in->rules, p),
                        p);
}

static AstDefine *
define_copy(AstDefine *in, apr_pool_t *p)
{
    return make_define(apr_pstrdup(p, in->name),
                       list_copy(in->keys, p),
                       list_copy_str(in->schema, p),
                       p);
}

static AstRule *
rule_copy(AstRule *in, apr_pool_t *p)
{
    return make_rule(apr_pstrdup(p, in->name),
                     in->is_delete,
                     in->is_network,
                     node_copy(in->head, p),
                     list_copy_deep(in->body, p),
                     p);
}

static AstFact *
fact_copy(AstFact *in, apr_pool_t *p)
{
    return make_fact(node_copy(in->head, p), p);
}

static AstTableRef *
table_ref_copy(AstTableRef *in, apr_pool_t *p)
{
    return make_table_ref(apr_pstrdup(p, in->name),
                          node_copy(in->cols, p),
                          p);
}

static AstColumnRef *
column_ref_copy(AstColumnRef *in, apr_pool_t *p)
{
    return make_column_ref(in->has_loc_spec,
                           node_copy(in->expr, p),
                           p);
}

static AstJoinClause *
join_clause_copy(AstJoinClause *in, apr_pool_t *p)
{
    return make_join_clause(node_copy(in->ref, p),
                            in->not, in->hash_variant,
                            p);
}

static AstQualifier *
qualifier_copy(AstQualifier *in, apr_pool_t *p)
{
    return make_qualifier(node_copy(in->expr, p), p);
}

static AstAssign *
assign_copy(AstAssign *in, apr_pool_t *p)
{
    return NULL;
}

static AstOpExpr *
op_expr_copy(AstOpExpr *in, apr_pool_t *p)
{
    return make_op_expr(node_copy(in->lhs, p),
                        node_copy(in->rhs, p),
                        in->op_kind, p);
}

static AstVarExpr *
var_expr_copy(AstVarExpr *in, apr_pool_t *p)
{
    return make_var_expr(apr_pstrdup(p, in->name), in->type, p);
}

void *
node_copy(void *ptr, apr_pool_t *pool)
{
    AstNode *n = (AstNode *) ptr;

    switch (n->kind)
    {
        case AST_PROGRAM:
            return program_copy((AstProgram *) n, pool);

        case AST_DEFINE:
            return define_copy((AstDefine *) n, pool);

        case AST_RULE:
            return rule_copy((AstRule *) n, pool);

        case AST_FACT:
            return fact_copy((AstFact *) n, pool);

        case AST_TABLE_REF:
            return table_ref_copy((AstTableRef *) n, pool);

        case AST_COLUMN_REF:
            return column_ref_copy((AstColumnRef *) n, pool);

        case AST_JOIN_CLAUSE:
            return join_clause_copy((AstJoinClause *) n, pool);

        case AST_QUALIFIER:
            return qualifier_copy((AstQualifier *) n, pool);

        case AST_ASSIGN:
            return assign_copy((AstAssign *) n, pool);

        case AST_OP_EXPR:
            return op_expr_copy((AstOpExpr *) n, pool);

        case AST_VAR_EXPR:
            return var_expr_copy((AstVarExpr *) n, pool);

        default:
            ERROR("Unrecognized node kind: %d", (int) n->kind);
            return NULL;        /* Keep compiler quiet */
    }
}
