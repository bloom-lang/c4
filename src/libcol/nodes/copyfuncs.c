#include "col-internal.h"
#include "nodes/copyfuncs.h"
#include "nodes/makefuncs.h"

static AstProgram *
copy_program(AstProgram *in, apr_pool_t *p)
{
    return make_program(apr_pstrdup(p, in->name),
                        list_copy_deep(in->defines, p),
                        list_copy_deep(in->facts, p),
                        list_copy_deep(in->rules, p),
                        p);
}

static AstDefine *
copy_define(AstDefine *in, apr_pool_t *p)
{
    return make_define(apr_pstrdup(p, in->name),
                       list_copy(in->keys, p),
                       list_copy_str(in->schema, p),
                       p);
}

static AstRule *
copy_rule(AstRule *in, apr_pool_t *p)
{
    return make_rule(apr_pstrdup(p, in->name),
                     in->is_delete,
                     in->is_network,
                     copy_node(in->head, p),
                     list_copy_deep(in->joins, p),
                     list_copy_deep(in->quals, p),
                     p);
}

static AstFact *
copy_fact(AstFact *in, apr_pool_t *p)
{
    return make_fact(copy_node(in->head, p), p);
}

static AstTableRef *
copy_table_ref(AstTableRef *in, apr_pool_t *p)
{
    return make_table_ref(apr_pstrdup(p, in->name),
                          list_copy_deep(in->cols, p),
                          p);
}

static AstColumnRef *
copy_column_ref(AstColumnRef *in, apr_pool_t *p)
{
    return make_column_ref(in->has_loc_spec,
                           copy_node(in->expr, p),
                           p);
}

static AstJoinClause *
copy_join_clause(AstJoinClause *in, apr_pool_t *p)
{
    return make_join_clause(copy_node(in->ref, p),
                            in->not, in->hash_variant,
                            p);
}

static AstQualifier *
copy_qualifier(AstQualifier *in, apr_pool_t *p)
{
    return make_qualifier(copy_node(in->expr, p), p);
}

static AstAssign *
copy_assign(AstAssign *in, apr_pool_t *p)
{
    return NULL;        /* XXX: TODO */
}

static AstOpExpr *
copy_op_expr(AstOpExpr *in, apr_pool_t *p)
{
    return make_op_expr(copy_node(in->lhs, p),
                        copy_node(in->rhs, p),
                        in->op_kind, p);
}

static AstVarExpr *
copy_var_expr(AstVarExpr *in, apr_pool_t *p)
{
    return make_var_expr(apr_pstrdup(p, in->name), in->type, p);
}

static AstConstExpr *
copy_const_expr(AstConstExpr *in, apr_pool_t *p)
{
    return make_const_expr(in->const_kind,
                           apr_pstrdup(p, in->value),
                           p);
}

static FilterPlan *
copy_filter_plan(FilterPlan *in, apr_pool_t *p)
{
    return make_filter_plan(list_copy_deep(in->quals, p),
                            apr_pstrdup(p, in->tbl_name),
                            p);
}

static InsertPlan *
copy_insert_plan(InsertPlan *in, apr_pool_t *p)
{
    return make_insert_plan(copy_node(in->head, p),
                            in->is_network, p);
}

static ScanPlan *
copy_scan_plan(ScanPlan *in, apr_pool_t *p)
{
    return make_scan_plan(list_copy_deep(in->quals, p),
                          apr_pstrdup(p, in->tbl_name),
                          p);
}

void *
copy_node(void *ptr, apr_pool_t *pool)
{
    ColNode *n = (ColNode *) ptr;

    switch (n->kind)
    {
        case AST_PROGRAM:
            return copy_program((AstProgram *) n, pool);

        case AST_DEFINE:
            return copy_define((AstDefine *) n, pool);

        case AST_RULE:
            return copy_rule((AstRule *) n, pool);

        case AST_FACT:
            return copy_fact((AstFact *) n, pool);

        case AST_TABLE_REF:
            return copy_table_ref((AstTableRef *) n, pool);

        case AST_COLUMN_REF:
            return copy_column_ref((AstColumnRef *) n, pool);

        case AST_JOIN_CLAUSE:
            return copy_join_clause((AstJoinClause *) n, pool);

        case AST_QUALIFIER:
            return copy_qualifier((AstQualifier *) n, pool);

        case AST_ASSIGN:
            return copy_assign((AstAssign *) n, pool);

        case AST_OP_EXPR:
            return copy_op_expr((AstOpExpr *) n, pool);

        case AST_VAR_EXPR:
            return copy_var_expr((AstVarExpr *) n, pool);

        case AST_CONST_EXPR:
            return copy_const_expr((AstConstExpr *) n, pool);

        case PLAN_FILTER:
            return copy_filter_plan((FilterPlan *) n, pool);

        case PLAN_INSERT:
            return copy_insert_plan((InsertPlan *) n, pool);

        case PLAN_SCAN:
            return copy_scan_plan((ScanPlan *) n, pool);

        default:
            ERROR("Unrecognized node kind: %d", (int) n->kind);
            return NULL;        /* Keep compiler quiet */
    }
}
