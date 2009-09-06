#include "col-internal.h"
#include "nodes/copyfuncs.h"
#include "nodes/makefuncs.h"

static AstProgram *
copy_program(AstProgram *in, apr_pool_t *p)
{
    return make_program(in->name, in->defines, in->facts, in->rules, p);
}

static AstDefine *
copy_define(AstDefine *in, apr_pool_t *p)
{
    return make_define(in->name, in->keys, in->schema, p);
}

static AstSchemaElt *
copy_schema_elt(AstSchemaElt *in, apr_pool_t *p)
{
    return make_schema_elt(in->type_name, in->is_loc_spec, p);
}

static AstRule *
copy_rule(AstRule *in, apr_pool_t *p)
{
    return make_rule(in->name, in->is_delete, in->is_network,
                     in->head, in->joins, in->quals, p);
}

static AstFact *
copy_fact(AstFact *in, apr_pool_t *p)
{
    return make_fact(in->head, p);
}

static AstTableRef *
copy_table_ref(AstTableRef *in, apr_pool_t *p)
{
    return make_table_ref(in->name, in->cols, p);
}

static AstColumnRef *
copy_column_ref(AstColumnRef *in, apr_pool_t *p)
{
    return make_column_ref(in->expr, p);
}

static AstJoinClause *
copy_join_clause(AstJoinClause *in, apr_pool_t *p)
{
    return make_join_clause(in->ref, in->not, in->hash_variant, p);
}

static AstQualifier *
copy_qualifier(AstQualifier *in, apr_pool_t *p)
{
    return make_qualifier(in->expr, p);
}

static AstOpExpr *
copy_ast_op_expr(AstOpExpr *in, apr_pool_t *p)
{
    return make_ast_op_expr(in->lhs, in->rhs, in->op_kind, p);
}

static AstVarExpr *
copy_ast_var_expr(AstVarExpr *in, apr_pool_t *p)
{
    return make_ast_var_expr(in->name, in->type, p);
}

static AstConstExpr *
copy_ast_const_expr(AstConstExpr *in, apr_pool_t *p)
{
    return make_ast_const_expr(in->const_kind, in->value, p);
}

static FilterPlan *
copy_filter_plan(FilterPlan *in, apr_pool_t *p)
{
    return make_filter_plan(in->tbl_name, in->plan.quals,
                            in->plan.qual_exprs, in->plan.proj_list, p);
}

static InsertPlan *
copy_insert_plan(InsertPlan *in, apr_pool_t *p)
{
    return make_insert_plan(in->head, in->plan.proj_list, in->is_network, p);
}

static ScanPlan *
copy_scan_plan(ScanPlan *in, apr_pool_t *p)
{
    return make_scan_plan(in->scan_rel, in->plan.quals,
                          in->plan.qual_exprs, in->plan.proj_list, p);
}

static ExprOp *
copy_expr_op(ExprOp *in, apr_pool_t *p)
{
    return make_expr_op(in->expr.type, in->op_kind, in->lhs, in->rhs, p);
}

static ExprVar *
copy_expr_var(ExprVar *in, apr_pool_t *p)
{
    return make_expr_var(in->expr.type, in->attno, in->is_outer, in->name, p);
}

static ExprConst *
copy_expr_const(ExprConst *in, apr_pool_t *p)
{
    return make_expr_const(in->expr.type, in->value, p);
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

        case AST_SCHEMA_ELT:
            return copy_schema_elt((AstSchemaElt *) n, pool);

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

        case AST_OP_EXPR:
            return copy_ast_op_expr((AstOpExpr *) n, pool);

        case AST_VAR_EXPR:
            return copy_ast_var_expr((AstVarExpr *) n, pool);

        case AST_CONST_EXPR:
            return copy_ast_const_expr((AstConstExpr *) n, pool);

        case PLAN_FILTER:
            return copy_filter_plan((FilterPlan *) n, pool);

        case PLAN_INSERT:
            return copy_insert_plan((InsertPlan *) n, pool);

        case PLAN_SCAN:
            return copy_scan_plan((ScanPlan *) n, pool);

        case EXPR_OP:
            return copy_expr_op((ExprOp *) n, pool);

        case EXPR_VAR:
            return copy_expr_var((ExprVar *) n, pool);

        case EXPR_CONST:
            return copy_expr_const((ExprConst *) n, pool);

        default:
            ERROR("Unrecognized node kind: %d", (int) n->kind);
            return NULL;        /* Keep compiler quiet */
    }
}
