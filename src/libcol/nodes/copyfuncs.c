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
copy_op_expr(AstOpExpr *in, apr_pool_t *p)
{
    return make_op_expr(in->lhs, in->rhs, in->op_kind, p);
}

static AstVarExpr *
copy_var_expr(AstVarExpr *in, apr_pool_t *p)
{
    return make_var_expr(in->name, in->type, p);
}

static AstConstExpr *
copy_const_expr(AstConstExpr *in, apr_pool_t *p)
{
    return make_const_expr(in->const_kind, in->value, p);
}

static ParseResult *
copy_parse_result(ParseResult *in, apr_pool_t *p)
{
    return make_parse_result(in->ast, in->range_tbl, p);
}

static RangeTblEntry *
copy_range_tbl_entry(RangeTblEntry *in, apr_pool_t *p)
{
    return make_range_tbl_entry(in->idx, in->rel_name, in->vars, p);
}

static FilterPlan *
copy_filter_plan(FilterPlan *in, apr_pool_t *p)
{
    return make_filter_plan(in->plan.quals, in->tbl_name, p);
}

static InsertPlan *
copy_insert_plan(InsertPlan *in, apr_pool_t *p)
{
    return make_insert_plan(in->head, in->is_network, p);
}

static ScanPlan *
copy_scan_plan(ScanPlan *in, apr_pool_t *p)
{
    return make_scan_plan(in->plan.quals, in->tbl_name, p);
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
            return copy_op_expr((AstOpExpr *) n, pool);

        case AST_VAR_EXPR:
            return copy_var_expr((AstVarExpr *) n, pool);

        case AST_CONST_EXPR:
            return copy_const_expr((AstConstExpr *) n, pool);

        case PARSE_RESULT:
            return copy_parse_result((ParseResult *) n, pool);

        case PARSE_RANGE_TBL_ENTRY:
            return copy_range_tbl_entry((RangeTblEntry *) n, pool);

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
