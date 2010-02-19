#include "c4-internal.h"
#include "nodes/copyfuncs.h"
#include "nodes/makefuncs.h"
#include "types/schema.h"

AstProgram *
make_program(List *defines, List *timers, List *facts, List *rules, apr_pool_t *p)
{
    AstProgram *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_PROGRAM;
    result->defines = list_copy_deep(defines, p);
    result->timers = list_copy_deep(timers, p);
    result->facts = list_copy_deep(facts, p);
    result->rules = list_copy_deep(rules, p);
    return result;
}

AstDefine *
make_define(const char *name, AstStorageKind storage,
            List *keys, List *schema, apr_pool_t *p)
{
    AstDefine *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_DEFINE;
    result->storage = storage;
    result->name = apr_pstrdup(p, name);
    result->keys = list_copy(keys, p);
    result->schema = list_copy_deep(schema, p);
    return result;
}

AstTimer *
make_ast_timer(const char *name, apr_int64_t period, apr_pool_t *p)
{
    AstTimer *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_TIMER;
    result->name = apr_pstrdup(p, name);
    result->period = period;
    return result;
}

AstSchemaElt *
make_schema_elt(const char *type_name, bool is_loc_spec, apr_pool_t *p)
{
    AstSchemaElt *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_SCHEMA_ELT;
    result->type_name = apr_pstrdup(p, type_name);
    result->is_loc_spec = is_loc_spec;
    return result;
}

AstRule *
make_rule(const char *name, bool is_delete, bool is_network,
          AstTableRef *head, List *joins, List *quals, apr_pool_t *p)
{
    AstRule *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_RULE;
    result->is_delete = is_delete;
    result->is_network = is_network;
    result->head = copy_node(head, p);

    if (name)
        result->name = apr_pstrdup(p, name);
    if (joins)
        result->joins = list_copy_deep(joins, p);
    if (quals)
        result->quals = list_copy_deep(quals, p);

    return result;
}

AstFact *
make_fact(AstTableRef *head, apr_pool_t *p)
{
    AstFact *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_FACT;
    result->head = copy_node(head, p);
    return result;
}

AstTableRef *
make_table_ref(const char *name, List *cols, apr_pool_t *p)
{
    AstTableRef *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_TABLE_REF;
    result->name = apr_pstrdup(p, name);
    result->cols = list_copy_deep(cols, p);
    return result;
}

AstColumnRef *
make_column_ref(C4Node *expr, apr_pool_t *p)
{
    AstColumnRef *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_COLUMN_REF;
    result->expr = copy_node(expr, p);
    return result;
}

AstJoinClause *
make_join_clause(AstTableRef *ref, bool not, apr_pool_t *p)
{
    AstJoinClause *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_JOIN_CLAUSE;
    result->ref = copy_node(ref, p);
    result->not = not;
    return result;
}

AstQualifier *
make_qualifier(C4Node *expr, apr_pool_t *p)
{
    AstQualifier *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_QUALIFIER;
    result->expr = copy_node(expr, p);
    return result;
}

AstOpExpr *
make_ast_op_expr(C4Node *lhs, C4Node *rhs, AstOperKind op_kind, apr_pool_t *p)
{
    AstOpExpr *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_OP_EXPR;
    result->op_kind = op_kind;
    result->lhs = copy_node(lhs, p);
    result->rhs = copy_node(rhs, p);
    return result;
}

AstVarExpr *
make_ast_var_expr(const char *name, DataType type, apr_pool_t *p)
{
    AstVarExpr *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_VAR_EXPR;
    result->name = apr_pstrdup(p, name);
    result->type = type;
    return result;
}

AstConstExpr *
make_ast_const_expr(AstConstKind c_kind, const char *value, apr_pool_t *p)
{
    AstConstExpr *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_CONST_EXPR;
    result->const_kind = c_kind;
    result->value = apr_pstrdup(p, value);
    return result;
}

AstAggExpr *
make_ast_agg_expr(AstAggKind a_kind, C4Node *expr, apr_pool_t *p)
{
    AstAggExpr *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_AGG_EXPR;
    result->agg_kind = a_kind;
    result->expr = copy_node(expr, p);
    return result;
}

AggPlan *
make_agg_plan(List *proj_list, apr_pool_t *p)
{
    AggPlan *result = apr_pcalloc(p, sizeof(*result));
    result->plan.node.kind = PLAN_AGG;
    result->plan.quals = list_make(p);

    if (proj_list)
        result->plan.proj_list = list_copy_deep(proj_list, p);
    return result;
}

FilterPlan *
make_filter_plan(const char *tbl_name, List *quals,
                 List *qual_exprs, List *proj_list, apr_pool_t *p)
{
    FilterPlan *result = apr_pcalloc(p, sizeof(*result));
    result->plan.node.kind = PLAN_FILTER;
    result->plan.quals = list_copy_deep(quals, p);
    if (qual_exprs)
        result->plan.qual_exprs = list_copy_deep(qual_exprs, p);
    if (proj_list)
        result->plan.proj_list = list_copy_deep(proj_list, p);
    result->tbl_name = apr_pstrdup(p, tbl_name);
    return result;
}

InsertPlan *
make_insert_plan(AstTableRef *head, bool do_delete,
                 List *proj_list, apr_pool_t *p)
{
    InsertPlan *result = apr_pcalloc(p, sizeof(*result));
    result->plan.node.kind = PLAN_INSERT;
    result->plan.quals = list_make(p);
    if (proj_list)
        result->plan.proj_list = list_copy_deep(proj_list, p);
    result->head = copy_node(head, p);
    result->do_delete = do_delete;
    return result;
}

ScanPlan *
make_scan_plan(AstJoinClause *scan_rel, List *quals,
               List *qual_exprs, List *proj_list, apr_pool_t *p)
{
    ScanPlan *result = apr_pcalloc(p, sizeof(*result));
    result->plan.node.kind = PLAN_SCAN;
    result->plan.quals = list_copy_deep(quals, p);
    if (qual_exprs)
        result->plan.qual_exprs = list_copy_deep(qual_exprs, p);
    if (proj_list)
        result->plan.proj_list = list_copy_deep(proj_list, p);
    result->scan_rel = copy_node(scan_rel, p);
    return result;
}

ExprOp *
make_expr_op(DataType type, AstOperKind op_kind,
             ExprNode *lhs, ExprNode *rhs, apr_pool_t *p)
{
    ExprOp *result = apr_pcalloc(p, sizeof(*result));
    result->expr.node.kind = EXPR_OP;
    result->expr.type = type;
    result->op_kind = op_kind;
    result->lhs = copy_node(lhs, p);
    result->rhs = copy_node(rhs, p);
    return result;
}

ExprVar *
make_expr_var(DataType type, int attno, bool is_outer,
              const char *name, apr_pool_t *p)
{
    ExprVar *result = apr_pcalloc(p, sizeof(*result));
    result->expr.node.kind = EXPR_VAR;
    result->expr.type = type;
    result->attno = attno;
    result->is_outer = is_outer;
    result->name = apr_pstrdup(p, name);
    return result;
}

ExprConst *
make_expr_const(DataType type, Datum val, apr_pool_t *p)
{
    ExprConst *result = apr_pcalloc(p, sizeof(*result));
    result->expr.node.kind = EXPR_CONST;
    result->expr.type = type;
    result->value = datum_copy(val, type);
    pool_track_datum(p, result->value, type);
    return result;
}
