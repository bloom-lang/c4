#include <apr_hash.h>

#include "c4-internal.h"
#include "nodes/makefuncs.h"
#include "parser/analyze.h"
#include "parser/walker.h"
#include "types/catalog.h"

typedef struct AnalyzeState
{
    apr_pool_t *pool;
    C4Runtime *c4;
    AstProgram *program;
    /* Map from table name => AstDefine */
    apr_hash_t *define_tbl;
    /* Map from rule name => AstRule */
    apr_hash_t *rule_tbl;

    /* Current index for system-generated rule names */
    int tmp_ruleno;

    /* Per-clause state: */
    /* Map from var name => AstVarExpr */
    apr_hash_t *var_tbl;
    /* Map from var name => AstJoinClause in which it is defined */
    apr_hash_t *var_join_tbl;
    /* Map from var name => list of equal variable names */
    apr_hash_t *eq_tbl;
    /* Current index for system-generated variable names */
    int tmp_varno;
} AnalyzeState;

/*
 * In what location does an expression appear? Certain constructs (e.g. don't
 * care variables) are legal in join clauses but not elsewhere, for example.
 */
typedef enum ExprLocation
{
    EXPR_LOC_HEAD,
    EXPR_LOC_JOIN,
    EXPR_LOC_QUAL
} ExprLocation;

static void analyze_table_ref(AstTableRef *ref, ExprLocation loc, AnalyzeState *state);
static void analyze_join_clause(AstJoinClause *join, AstRule *rule,
                                AnalyzeState *state);
static void analyze_qualifier(AstQualifier *qual, AnalyzeState *state);
static void analyze_expr(C4Node *node, ExprLocation loc, AnalyzeState *state);
static void analyze_op_expr(AstOpExpr *op_expr, ExprLocation loc,
                            AnalyzeState *state);
static void analyze_var_expr(AstVarExpr *var_expr, ExprLocation loc,
                             AnalyzeState *state);
static void analyze_const_expr(AstConstExpr *c_expr, AnalyzeState *state);
static void analyze_agg_expr(AstAggExpr *a_expr, ExprLocation loc, AnalyzeState *state);
static void analyze_rule_head(AstRule *rule, AnalyzeState *state);
static void analyze_rule_location(AstRule *rule, AnalyzeState *state);

static void find_unused_vars(AstRule *rule, AnalyzeState *state);
static void check_rule_safety(AstRule *rule, AnalyzeState *state);
static C4Node *table_ref_get_loc_spec(AstTableRef *ref,
                                            AnalyzeState *state);
static bool loc_spec_equal(C4Node *ls1, C4Node *ls2, AnalyzeState *state);
static char *make_anon_rule_name(AnalyzeState *state);
static bool is_rule_defined(const char *name, AnalyzeState *state);
static bool is_var_defined(const char *name, AnalyzeState *state);
static AstVarExpr *lookup_var(const char *name, AnalyzeState *state);
static void define_var(AstVarExpr *var, AstJoinClause *join, AnalyzeState *state);
static bool is_var_equal(AstVarExpr *v1, AstVarExpr *v2, AnalyzeState *state);
static char *make_anon_var_name(const char *prefix, AnalyzeState *state);
static void add_qual(char *lhs_name, C4Node *rhs, AstOperKind op_kind,
                     AstRule *rule, AnalyzeState *state);
static bool is_dont_care_var(C4Node *node);
static void make_var_eq_table(AstRule *rule, AnalyzeState *state);
static void make_implied_quals(AstRule *rule, AnalyzeState *state);
static bool table_is_defined(const char *tbl_name, AnalyzeState *state);
static DataType table_get_col_type(const char *tbl_name, int colno, AnalyzeState *state);
static int table_get_num_cols(const char *tbl_name, AnalyzeState *state);

static DataType op_expr_get_type(AstOpExpr *op_expr);
static DataType const_expr_get_type(AstConstExpr *c_expr);
static DataType var_expr_get_type(AstVarExpr *var);
static DataType agg_expr_get_type(AstAggExpr *agg);


static void
analyze_define(AstDefine *def, AnalyzeState *state)
{
    List *seen_keys;
    bool seen_loc_spec;
    ListCell *lc;

    if (table_is_defined(def->name, state))
        ERROR("Table %s already exists", def->name);

    apr_hash_set(state->define_tbl, def->name,
                 APR_HASH_KEY_STRING, def);

    /* Validate the table's schema */
    seen_loc_spec = false;
    foreach (lc, def->schema)
    {
        AstSchemaElt *elt = (AstSchemaElt *) lc_ptr(lc);

        if (!is_valid_type_name(elt->type_name))
            ERROR("Undefined type %s in schema of table %s",
                  elt->type_name, def->name);

        if (elt->is_loc_spec)
        {
            DataType type;

            if (seen_loc_spec)
                ERROR("Schema of table %s cannot have more "
                      "than one location specifier", def->name);

            seen_loc_spec = true;
            type = get_type_id(elt->type_name);
            if (type != TYPE_STRING)
                ERROR("Location specifiers must be of type string");
        }
    }

    /* Validate the keys list */
    if (list_length(def->keys) > list_length(def->schema))
        ERROR("Key list exceeds schema length of table %s",
              def->name);

    seen_keys = list_make(state->pool);
    foreach (lc, def->keys)
    {
        int key = lc_int(lc);

        if (key < 0)
            ERROR("Negative key %d in table %s", key, def->name);

        if (list_member_int(seen_keys, key))
            ERROR("Duplicate key %d in table %s", key, def->name);

        if (key >= list_length(def->schema))
            ERROR("Key index %d exceeds schema length in table %s",
                  key, def->name);

        list_append_int(seen_keys, key);
    }
}

static void
analyze_timer(AstTimer *timer, AnalyzeState *state)
{
    List *keys;
    List *schema;
    AstDefine *def;

    if (timer->period <= 0)
        ERROR("Period of timer %s is %" APR_INT64_T_FMT "; must be positive",
              timer->name, timer->period);

    if (timer->period > (APR_INT64_MAX / 1000))
        ERROR("Period of timer %s is too large", timer->name);

    /* Add an AstDefine for the timer table to the AST */
    keys = list_make1_int(0, state->pool);
    schema = list_make(state->pool);
    list_append(schema, make_schema_elt("int8", false, state->pool));

    def = make_define(timer->name, AST_STORAGE_MEMORY, keys, schema, state->pool);
    list_append(state->program->defines, def);
    analyze_define(def, state);
}

static void
analyze_rule(AstRule *rule, AnalyzeState *state)
{
    ListCell *lc;

    /*
     * If no rule name provided, generate a unique name. Otherwise, check that
     * the rule name is unique within the current program.
     */
    if (!rule->name)
        rule->name = make_anon_rule_name(state);
    else if (is_rule_defined(rule->name, state))
        ERROR("Duplicate rule name: %s", rule->name);

    apr_hash_set(state->rule_tbl, rule->name, APR_HASH_KEY_STRING, rule);

    /*
     * Check the rule body. Consider the join clauses first, and then
     * examine the qualifiers (all join variables are "in-scope" for
     * qualifiers, even if they appear before the join in the program text).
     */
    foreach (lc, rule->joins)
    {
        AstJoinClause *join = (AstJoinClause *) lc_ptr(lc);

        analyze_join_clause(join, rule, state);
    }

    foreach (lc, rule->quals)
    {
        AstQualifier *qual = (AstQualifier *) lc_ptr(lc);

        analyze_qualifier(qual, state);
    }

    analyze_rule_head(rule, state);

    make_var_eq_table(rule, state);
    make_implied_quals(rule, state);
    analyze_rule_location(rule, state);
    find_unused_vars(rule, state);
    check_rule_safety(rule, state);
}

static bool
disallow_agg_walker(C4Node *n, __unused void *data)
{
    if (n->kind == AST_AGG_EXPR)
        ERROR("Aggregate used in illegal context");

    return true;
}

static void
analyze_rule_head(AstRule *rule, AnalyzeState *state)
{
    ListCell *lc;

    ASSERT(!rule->has_agg);
    analyze_table_ref(rule->head, EXPR_LOC_HEAD, state);

    foreach (lc, rule->head->cols)
    {
        C4Node *expr = (C4Node *) lc_ptr(lc);

        /*
         * We currently require that aggs not be nested within other expressions
         * in the rule head.
         */
        if (expr->kind == AST_AGG_EXPR)
        {
            rule->has_agg = true;
            continue;
        }

        expr_tree_walker(expr, disallow_agg_walker, NULL);
    }
}

/*
 * Check the consistency of the location specifiers used in the rule, and
 * determine whether it is a network rule.
 */
static void
analyze_rule_location(AstRule *rule, AnalyzeState *state)
{
    C4Node *body_loc_spec;
    C4Node *head_loc_spec;
    ListCell *lc;

    /*
     * NB: At present, at most one distinct location specifier can be used
     * in the rule body.
     */
    body_loc_spec = NULL;
    foreach (lc, rule->joins)
    {
        AstJoinClause *join = (AstJoinClause *) lc_ptr(lc);
        C4Node *loc_spec;

        loc_spec = table_ref_get_loc_spec(join->ref, state);
        if (loc_spec != NULL)
        {
            if (body_loc_spec != NULL &&
                !loc_spec_equal(loc_spec, body_loc_spec, state))
                ERROR("Distinct location specifiers in the body "
                      "of a single rule are not allowed");

            body_loc_spec = loc_spec;
        }
    }

    /*
     * If a location specifier is used in the body that is distinct from the
     * head's location specifier, this is a network rule.
     */
    head_loc_spec = table_ref_get_loc_spec(rule->head, state);
    if (body_loc_spec != NULL && head_loc_spec != NULL &&
        !loc_spec_equal(body_loc_spec, head_loc_spec, state))
        rule->is_network = true;
}

static bool
unused_var_walker(AstVarExpr *var, void *data)
{
    apr_hash_t *var_seen = (apr_hash_t *) data;

    apr_hash_set(var_seen, var->name,
                 APR_HASH_KEY_STRING, var->name);

    return true;
}

/*
 * Search for variables that are defined by a join clause, but are not
 * referenced by a qual or the rule head. Such a variable need not be assigned a
 * name, so we emit a warning.
 */
static void
find_unused_vars(AstRule *rule, AnalyzeState *state)
{
    apr_hash_t *var_seen;
    ListCell *lc;
    apr_hash_index_t *hi;

    var_seen = apr_hash_make(state->pool);

    expr_tree_var_walker((C4Node *) rule->head, unused_var_walker, var_seen);

    foreach (lc, rule->quals)
    {
        C4Node *qual = (C4Node *) lc_ptr(lc);

        expr_tree_var_walker(qual, unused_var_walker, var_seen);
    }

    for (hi = apr_hash_first(state->pool, state->var_tbl);
         hi != NULL; hi = apr_hash_next(hi))
    {
        const char *var_name;

        apr_hash_this(hi, (const void **) &var_name, NULL, NULL);

        if (apr_hash_get(var_seen, var_name,
                         APR_HASH_KEY_STRING) == NULL)
        {
            printf("Unused variable name: %s\n", var_name);
        }
    }
}

static bool
check_negation_walker(AstVarExpr *var, void *data)
{
    AnalyzeState *state = (AnalyzeState *) data;
    List *eq_list;
    ListCell *lc;

    eq_list = get_eq_list(var->name, false, state->eq_tbl, state->pool);
    foreach (lc, eq_list)
    {
        char *eq_var_name = lc_ptr(lc);
        AstJoinClause *join;

        join = apr_hash_get(state->var_join_tbl, eq_var_name,
                            APR_HASH_KEY_STRING);
        if (!join->not)
            return true;
    }

    ERROR("Variable \"%s\" in head appears only in negated body terms",
          var->name);
}

static void
check_rule_safety(AstRule *rule, AnalyzeState *state)
{
    ListCell *lc;

    /*
     * For each variable in the rule head, check that there is at least one
     * variable it is equal to that appears in a non-negated join clause.
     * Otherwise, the rule could produce unbounded output: "x(A) :- notin y(A);"
     * is unsafe.
     */
    foreach (lc, rule->head->cols)
    {
        C4Node *expr = (C4Node *) lc_ptr(lc);

        expr_tree_var_walker(expr, check_negation_walker, state);
    }

    /*
     * Check that there is at least one non-negated join clause in the
     * body. This is almost a subset of the previous check, except that a rule
     * might contain only constant values in its head.
     */
    foreach (lc, rule->joins)
    {
        AstJoinClause *join = (AstJoinClause *) lc_ptr(lc);

        if (!join->not)
            return;
    }

    ERROR("Rule body must contain at least one non-negated join clause");
}

static int
table_get_loc_spec_colno(const char *tbl_name, AnalyzeState *state)
{
    AstDefine *define;
    int colno;
    ListCell *lc;

    if (cat_table_exists(state->c4->cat, tbl_name))
    {
        TableDef *tbl_def = cat_get_table(state->c4->cat, tbl_name);
        return tbl_def->ls_colno;
    }

    define = apr_hash_get(state->define_tbl, tbl_name, APR_HASH_KEY_STRING);
    colno = 0;
    foreach (lc, define->schema)
    {
        AstSchemaElt *elt = (AstSchemaElt *) lc_ptr(lc);

        if (elt->is_loc_spec)
            return colno;

        colno++;
    }

    return -1;
}

static C4Node *
table_ref_get_loc_spec(AstTableRef *ref, AnalyzeState *state)
{
    int colno;

    colno = table_get_loc_spec_colno(ref->name, state);
    if (colno == -1)
        return NULL;

    return (C4Node *) list_get(ref->cols, colno);
}

static bool
loc_spec_equal(C4Node *ls1, C4Node *ls2, AnalyzeState *state)
{
    /* Only variable location specifiers supported for now */
    ASSERT(ls1->kind == AST_VAR_EXPR);
    ASSERT(ls2->kind == AST_VAR_EXPR);

    /*
     * NB: This only checks if there is an explicit or implied equality
     * constraint between the two variable names. In practice, this is
     * probably sufficient.
     */
    return is_var_equal((AstVarExpr *) ls1, (AstVarExpr *) ls2, state);
}

static char *
make_anon_rule_name(AnalyzeState *state)
{
    char rule_name[128];

    while (true)
    {
        snprintf(rule_name, sizeof(rule_name), "r_%d_sys",
                 state->tmp_ruleno++);

        if (!is_rule_defined(rule_name, state))
            return apr_pstrdup(state->pool, rule_name);
    }
}

static bool
is_rule_defined(const char *name, AnalyzeState *state)
{
    return (bool) (apr_hash_get(state->rule_tbl, name,
                                APR_HASH_KEY_STRING) != NULL);
}

static bool
is_var_defined(const char *name, AnalyzeState *state)
{
    return (bool) (lookup_var(name, state) != NULL);
}

static AstVarExpr *
lookup_var(const char *name, AnalyzeState *state)
{
    return (AstVarExpr *) apr_hash_get(state->var_tbl, name,
                                       APR_HASH_KEY_STRING);
}

static void
define_var(AstVarExpr *var, AstJoinClause *join, AnalyzeState *state)
{
    ASSERT(!is_dont_care_var((C4Node *) var));
    ASSERT(!is_var_defined(var->name, state));
    apr_hash_set(state->var_tbl, var->name,
                 APR_HASH_KEY_STRING, var);
    apr_hash_set(state->var_join_tbl, var->name,
                 APR_HASH_KEY_STRING, join);
}

static bool
is_var_equal(AstVarExpr *v1, AstVarExpr *v2, AnalyzeState *state)
{
    List *eq_list;

    eq_list = get_eq_list(v1->name, false, state->eq_tbl, state->pool);
    return list_member_str(eq_list, v2->name);
}

static char *
make_anon_var_name(const char *prefix, AnalyzeState *state)
{
    char var_name[128];

    while (true)
    {
        snprintf(var_name, sizeof(var_name), "%s_%d",
                 prefix, state->tmp_varno++);

        if (!is_var_defined(var_name, state))
            return apr_pstrdup(state->pool, var_name);
    }
}

static void
analyze_join_clause(AstJoinClause *join, AstRule *rule, AnalyzeState *state)
{
    int colno;
    ListCell *lc;

    if (!table_is_defined(join->ref->name, state))
        ERROR("No such table \"%s\"", join->ref->name);

    colno = 0;
    foreach (lc, join->ref->cols)
    {
        C4Node *expr = (C4Node *) lc_ptr(lc);
        AstVarExpr *var;

        /*
         * Replace constants in join clauses with a system-generated variable,
         * and add an equality qualifier between the new variable and the
         * constant value.
         */
        if (expr->kind == AST_CONST_EXPR)
        {
            char *var_name;

            var_name = make_anon_var_name("Const", state);
            add_qual(var_name, expr, AST_OP_EQ, rule, state);

            /* Type will be filled-in shortly */
            var = make_ast_var_expr(var_name, TYPE_INVALID, state->pool);
            lc_ptr(lc) = var;
            expr = (C4Node *) var;
        }
        else
        {
            /*
             * OpExprs and AggExprs are disallowed. This might be relaxed in the
             * future.
             */
            if (expr->kind != AST_VAR_EXPR)
                ERROR("A join clause in the rule body must contain "
                      "either variables or constants");

            if (is_dont_care_var(expr))
                goto done;

            var = (AstVarExpr *) expr;

            /*
             * Check if the variable name has already been used by a
             * JoinClause in this rule.  If it has, then assign a new
             * system-generated name to the variable, and add an equality
             * qualifier between the new and old variable names.
             */
            if (is_var_defined(var->name, state))
            {
                char *old_name = var->name;

                var->name = make_anon_var_name(var->name, state);
                add_qual(old_name, (C4Node *) var,
                         AST_OP_EQ, rule, state);
            }
        }

done:
        /*
         * We are left with either a uniquely-occurring variable or a don't
         * care variable.
         */
        ASSERT(expr->kind == AST_VAR_EXPR);
        var = (AstVarExpr *) expr;

        /* Fill-in variable's type, add to variable tables (unless don't care) */
        var->type = table_get_col_type(join->ref->name, colno, state);
        if (!is_dont_care_var((C4Node *) var))
            define_var(var, join, state);
        colno++;
    }

    analyze_table_ref(join->ref, EXPR_LOC_JOIN, state);
}

static void
add_qual(char *lhs_name, C4Node *rhs, AstOperKind op_kind,
         AstRule *rule, AnalyzeState *state)
{
    AstVarExpr *lhs;
    AstOpExpr *op_expr;
    AstQualifier *qual;

    lhs = make_ast_var_expr(lhs_name, TYPE_INVALID, state->pool);
    op_expr = make_ast_op_expr((C4Node *) lhs, rhs,
                               op_kind, state->pool);
    qual = make_qualifier((C4Node *) op_expr, state->pool);

    list_append(rule->quals, qual);
}

static void
analyze_qualifier(AstQualifier *qual, AnalyzeState *state)
{
    DataType type;

    analyze_expr(qual->expr, EXPR_LOC_QUAL, state);

    type = expr_get_type(qual->expr);
    if (type != TYPE_BOOL)
        ERROR("Qualifier must have boolean result, not %s",
              get_type_name(type));
}

static void
analyze_expr(C4Node *node, ExprLocation loc, AnalyzeState *state)
{
    switch (node->kind)
    {
        case AST_OP_EXPR:
            analyze_op_expr((AstOpExpr *) node, loc, state);
            break;

        case AST_VAR_EXPR:
            analyze_var_expr((AstVarExpr *) node, loc, state);
            break;

        case AST_CONST_EXPR:
            analyze_const_expr((AstConstExpr *) node, state);
            break;

        case AST_AGG_EXPR:
            analyze_agg_expr((AstAggExpr *) node, loc, state);
            break;

        default:
            ERROR("Unexpected expr node kind: %d", (int) node->kind);
    }
}

static void
analyze_op_expr(AstOpExpr *op_expr, ExprLocation loc, AnalyzeState *state)
{
    DataType lhs_type;
    DataType rhs_type;

    analyze_expr(op_expr->lhs, loc, state);
    lhs_type = expr_get_type(op_expr->lhs);

    /* Unary operators are simple */
    if (op_expr->op_kind == AST_OP_UMINUS)
    {
        if (!is_numeric_type(lhs_type))
            ERROR("Unary minus must have numeric input");

        return;
    }

    analyze_expr(op_expr->rhs, loc, state);
    rhs_type = expr_get_type(op_expr->rhs);

    /* XXX: type compatibility check is far too strict */
    if (lhs_type != rhs_type)
        ERROR("Type mismatch in qualifier: lhs is %s, rhs is %s",
              get_type_name(lhs_type), get_type_name(rhs_type));

    switch (op_expr->op_kind)
    {
        case AST_OP_PLUS:
            if (lhs_type == TYPE_STRING && rhs_type == TYPE_STRING)
                break;
            /* Fall through */

        case AST_OP_MINUS:
        case AST_OP_TIMES:
        case AST_OP_DIVIDE:
        case AST_OP_MODULUS:
            if (!is_numeric_type(lhs_type) || !is_numeric_type(rhs_type))
                ERROR("Numeric operator must have numeric input");
            break;

        default:
            break;
    }
}

static void
analyze_var_expr(AstVarExpr *var_expr, ExprLocation loc, AnalyzeState *state)
{
    if (is_dont_care_var((C4Node *) var_expr))
    {
        if (loc == EXPR_LOC_QUAL)
            ERROR("Don't care variables (\"_\") cannot be "
                  "referenced in qualifiers");
        if (loc == EXPR_LOC_HEAD)
            ERROR("Don't care variables (\"_\") cannot appear "
                  "in the head of a rule");

        /*
         * For don't care variables in join clauses, we're done: we don't want
         * to lookup the variable's name, and we should have already computed
         * the variable's type.
         */
        ASSERT(var_expr->type != TYPE_INVALID);
        return;
    }

    if (!is_var_defined(var_expr->name, state))
        ERROR("Undefined variable \"%s\"", var_expr->name);

    /*
     * If the type for the var has not yet been filled-in, do so now by
     * looking at the variable table. Once analyze_expr() has been called on
     * an expression tree, it is safe to call expr_get_type() on it.
     */
    if (var_expr->type == TYPE_INVALID)
    {
        AstVarExpr *def_var;

        def_var = lookup_var(var_expr->name, state);
        ASSERT(def_var != NULL);
        ASSERT(def_var->type != TYPE_INVALID);

        var_expr->type = def_var->type;
    }
}

static void
analyze_const_expr(AstConstExpr *c_expr, AnalyzeState *state)
{
    ;
}

static void
analyze_agg_expr(AstAggExpr *a_expr, ExprLocation loc, AnalyzeState *state)
{
    if (loc != EXPR_LOC_HEAD)
        ERROR("Aggregates must be used in rule heads");

    analyze_expr(a_expr->expr, loc, state);

    /* Check consistency of input to aggregate function */
    switch (a_expr->agg_kind)
    {
        case AST_AGG_AVG:
            if (expr_get_type(a_expr->expr) != TYPE_DOUBLE)
                ERROR("Avg aggregate must be used with double value");
            break;

        case AST_AGG_SUM:
            if (expr_get_type(a_expr->expr) != TYPE_INT8)
                ERROR("Sum aggregate must be used with int8 value");
            break;

        default:
            break;
    }
}

static void
analyze_fact(AstFact *fact, AnalyzeState *state)
{
    AstTableRef *target = fact->head;
    ListCell *lc;

    analyze_table_ref(target, EXPR_LOC_HEAD, state);

    foreach (lc, target->cols)
    {
        C4Node *expr = (C4Node *) lc_ptr(lc);

        if (expr->kind != AST_CONST_EXPR)
            ERROR("Columns in a fact must be constants");
    }
}

static void
analyze_table_ref(AstTableRef *ref, ExprLocation loc, AnalyzeState *state)
{
    int num_cols;
    ListCell *lc;
    int colno;

    /* Check that the referenced table exists */
    if (!table_is_defined(ref->name, state))
        ERROR("No such table \"%s\"", ref->name);

    num_cols = table_get_num_cols(ref->name, state);

    /*
     * Check that the data types of the table reference are compatible with
     * the schema of the table
     */
    if (num_cols != list_length(ref->cols))
        ERROR("Reference to table %s has incorrect # of columns. "
              "Expected: %d, encountered %d",
              ref->name, num_cols, list_length(ref->cols));

    colno = 0;
    foreach (lc, ref->cols)
    {
        C4Node *expr = (C4Node *) lc_ptr(lc);
        DataType expr_type;
        DataType schema_type;

        analyze_expr(expr, loc, state);
        expr_type = expr_get_type(expr);
        schema_type = table_get_col_type(ref->name, colno, state);

        /* XXX: type compatibility check is far too strict */
        if (schema_type != expr_type)
            ERROR("Type mismatch in column list: %s in schema, %s in column list",
                  get_type_name(schema_type), get_type_name(expr_type));

        colno++;
    }
}

static bool
is_dont_care_var(C4Node *node)
{
    AstVarExpr *var;

    if (node->kind != AST_VAR_EXPR)
        return false;

    var = (AstVarExpr *) node;
    return (bool) (strcmp(var->name, "_") == 0);
}

List *
get_eq_list(const char *var_name, bool make_new, apr_hash_t *eq_tbl, apr_pool_t *p)
{
    List *eq_list;

    eq_list = apr_hash_get(eq_tbl, var_name, APR_HASH_KEY_STRING);
    if (eq_list == NULL)
    {
        if (!make_new)
            ERROR("Failed to find equality list for variable %s", var_name);

        eq_list = list_make(p);
        apr_hash_set(eq_tbl, var_name, APR_HASH_KEY_STRING, eq_list);
    }

    return eq_list;
}

/* Record that "v2" is an alias for "v1". */
void
add_var_eq(char *v1, char *v2, apr_hash_t *eq_tbl, apr_pool_t *p)
{
    List *eq_list;

    eq_list = get_eq_list(v1, true, eq_tbl, p);
    list_append(eq_list, v2);
}

/*
 * We use a simple definition of "equality": we look for an op expression
 * consisting of "=" between two bare variables.
 */
bool
is_simple_equality(AstQualifier *qual, AstVarExpr **lhs, AstVarExpr **rhs)
{
    AstOpExpr *op_expr;

    if (qual->expr->kind != AST_OP_EXPR)
        return false;

    op_expr = (AstOpExpr *) qual->expr;
    if (op_expr->op_kind != AST_OP_EQ)
        return false;

    if (op_expr->lhs->kind != AST_VAR_EXPR ||
        op_expr->rhs->kind != AST_VAR_EXPR)
        return false;

    *lhs = (AstVarExpr *) op_expr->lhs;
    *rhs = (AstVarExpr *) op_expr->rhs;
    return true;
}

static void
make_var_eq_table(AstRule *rule, AnalyzeState *state)
{
    apr_hash_index_t *hi;
    ListCell *lc;

    ASSERT(apr_hash_count(state->eq_tbl) == 0);

    /*
     * Add an entry to the variable equality table to record that each
     * variable is equal to itself.
     */
    for (hi = apr_hash_first(state->pool, state->var_tbl);
         hi != NULL; hi = apr_hash_next(hi))
    {
        char *var_name;

        apr_hash_this(hi, (const void **) &var_name, NULL, NULL);
        add_var_eq(var_name, var_name, state->eq_tbl, state->pool);
    }

    /*
     * Add variable equality entries to reflect the explicit simple equality
     * quals from the rule body.
     */
    foreach (lc, rule->quals)
    {
        AstQualifier *qual = (AstQualifier *) lc_ptr(lc);
        AstVarExpr *lhs;
        AstVarExpr *rhs;

        if (!is_simple_equality(qual, &lhs, &rhs))
            continue;

        add_var_eq(lhs->name, rhs->name, state->eq_tbl, state->pool);
        add_var_eq(rhs->name, lhs->name, state->eq_tbl, state->pool);
    }
}

static int
add_qual_list(AstVarExpr *target, List *var_list, AstRule *rule,
              AnalyzeState *state)
{
    ListCell *lc;
    int count;

    count = 0;
    foreach (lc, var_list)
    {
        char *var = (char *) lc_ptr(lc);
        List *eq_list;

        if (strcmp(var, target->name) == 0)
            continue;

        eq_list = get_eq_list(var, false, state->eq_tbl, state->pool);
        if (!list_member_str(eq_list, target->name))
        {
            /* Create the actual qual */
            AstVarExpr *rhs;
            AstOpExpr *op_expr;
            AstQualifier *qual;

            rhs = lookup_var(var, state);
            op_expr = make_ast_op_expr((C4Node *) target,
                                       (C4Node *) rhs,
                                       AST_OP_EQ,
                                       state->pool);
            qual = make_qualifier((C4Node *) op_expr, state->pool);
            list_append(rule->quals, qual);

            add_var_eq(var, target->name, state->eq_tbl, state->pool);
            add_var_eq(target->name, var, state->eq_tbl, state->pool);
            list_append(eq_list, target->name);
            count++;
        }
    }

    return count;
}

/*
 * Check whether a table with the given name exists. We check for both
 * tables defined by the current program, and previously-defined tables that
 * already exist in the catalog.
 */
static bool
table_is_defined(const char *tbl_name, AnalyzeState *state)
{
    if (apr_hash_get(state->define_tbl, tbl_name,
                     APR_HASH_KEY_STRING) != NULL)
        return true;

    /* Check for an already-defined table of the same name */
    if (cat_table_exists(state->c4->cat, tbl_name))
        return true;

    return false;
}

/*
 * Returns the DataType of column "colno" from the table with the given
 * name. This is ugly, because the data structure we need to consult depends
 * on whether the table is already defined in the catalog or not.
 */
static DataType
table_get_col_type(const char *tbl_name, int colno, AnalyzeState *state)
{
    AstDefine *define;
    TableDef *tbl_def;

    define = apr_hash_get(state->define_tbl, tbl_name, APR_HASH_KEY_STRING);
    if (define != NULL)
    {
        AstSchemaElt *elt = (AstSchemaElt *) list_get(define->schema, colno);
        return get_type_id(elt->type_name);
    }

    tbl_def = cat_get_table(state->c4->cat, tbl_name);
    return schema_get_type(tbl_def->schema, colno);
}

static int
table_get_num_cols(const char *tbl_name, AnalyzeState *state)
{
    AstDefine *define;
    TableDef *tbl_def;

    define = apr_hash_get(state->define_tbl, tbl_name, APR_HASH_KEY_STRING);
    if (define != NULL)
        return list_length(define->schema);

    tbl_def = cat_get_table(state->c4->cat, tbl_name);
    return tbl_def->schema->len;
}

/*
 * Compute qualifiers that are implied by the set of quals explicitly stated
 * in the query. We only try to do this for the trivial case of computing
 * the transitive closure over the simple var-equality quals: that is, given
 * A = B and B = C, we generate A = C.
 *
 * This is useful because it gives the planner more choice for join order. It is
 * also necessary for counting the number of distinct location specifiers in the
 * rule.
 *
 * XXX: We currently do this via a braindead algorithm.
 */
static void
make_implied_quals(AstRule *rule, AnalyzeState *state)
{
    ListCell *lc;
    int count;

top:
    count = 0;
    foreach (lc, rule->quals)
    {
        AstQualifier *qual = (AstQualifier *) lc_ptr(lc);
        AstVarExpr *lhs;
        AstVarExpr *rhs;
        List *eq_list;

        if (!is_simple_equality(qual, &lhs, &rhs))
            continue;

        eq_list = get_eq_list(lhs->name, false, state->eq_tbl, state->pool);
        count += add_qual_list(rhs, eq_list, rule, state);

        eq_list = get_eq_list(rhs->name, false, state->eq_tbl, state->pool);
        count += add_qual_list(lhs, eq_list, rule, state);
    }

    if (count > 0)
        goto top;
}

DataType
expr_get_type(C4Node *node)
{
    switch (node->kind)
    {
        case AST_OP_EXPR:
            return op_expr_get_type((AstOpExpr *) node);

        case AST_VAR_EXPR:
            return var_expr_get_type((AstVarExpr *) node);

        case AST_CONST_EXPR:
            return const_expr_get_type((AstConstExpr *) node);

        case AST_AGG_EXPR:
            return agg_expr_get_type((AstAggExpr *) node);

        default:
            ERROR("Unexpected node kind: %d", (int) node->kind);
    }
}

static DataType
op_expr_get_type(AstOpExpr *op_expr)
{
    switch (op_expr->op_kind)
    {
        case AST_OP_LT:
        case AST_OP_LTE:
        case AST_OP_GT:
        case AST_OP_GTE:
        case AST_OP_EQ:
        case AST_OP_NEQ:
            return TYPE_BOOL;

        case AST_OP_UMINUS:
            return expr_get_type(op_expr->lhs);

        case AST_OP_PLUS:
        case AST_OP_MINUS:
        case AST_OP_TIMES:
        case AST_OP_DIVIDE:
        case AST_OP_MODULUS:
        {
            DataType lhs_type = expr_get_type(op_expr->lhs);

            /* XXX: probably too strict; need to allow implicit casts */
            ASSERT(lhs_type == expr_get_type(op_expr->rhs));
            return lhs_type;
        }

        default:
            ERROR("Unexpected operator kind: %d", (int) op_expr->op_kind);
    }
}

static DataType
const_expr_get_type(AstConstExpr *c_expr)
{
    switch (c_expr->const_kind)
    {
        case AST_CONST_BOOL:
            return TYPE_BOOL;

        case AST_CONST_CHAR:
            return TYPE_CHAR;

        case AST_CONST_DOUBLE:
            return TYPE_DOUBLE;

        case AST_CONST_INT:
            return TYPE_INT8;

        case AST_CONST_STRING:
            return TYPE_STRING;

        default:
            ERROR("Unexpected const kind: %d", (int) c_expr->const_kind);
    }
}

/*
 * Note that we can only use the DataType in the VarExpr itself if this
 * routine is called after analyze_expr() has been invoked on the expression
 * subtree.
 */
static DataType
var_expr_get_type(AstVarExpr *var)
{
    ASSERT(var->type != TYPE_INVALID);
    return var->type;
}

static DataType
agg_expr_get_type(AstAggExpr *agg)
{
    switch (agg->agg_kind)
    {
        case AST_AGG_AVG:
            return TYPE_DOUBLE;

        case AST_AGG_COUNT:
        case AST_AGG_SUM:
            return TYPE_INT8;

        case AST_AGG_MAX:
        case AST_AGG_MIN:
            return expr_get_type(agg->expr);

        default:
            ERROR("Unexpected agg kind: %d", (int) agg->agg_kind);
    }
}

/*
 * Invoke the semantic analyzer on the specified program. Note that the
 * analysis phase is side-effecting: the input AstProgram is destructively
 * modified.
 */
void
analyze_ast(AstProgram *program, apr_pool_t *pool, C4Runtime *c4)
{
    AnalyzeState *state;
    ListCell *lc;

    state = apr_palloc(pool, sizeof(*state));
    state->pool = pool;
    state->c4 = c4;
    state->program = program;
    state->define_tbl = apr_hash_make(pool);
    state->rule_tbl = apr_hash_make(pool);
    state->var_tbl = apr_hash_make(pool);
    state->var_join_tbl = apr_hash_make(pool);
    state->eq_tbl = apr_hash_make(pool);
    state->tmp_ruleno = 0;

    /* Phase 1: process table definitions */
    foreach (lc, program->defines)
    {
        AstDefine *def = (AstDefine *) lc_ptr(lc);

        analyze_define(def, state);
    }

    /* Phase 2: process timer definitions */
    foreach (lc, program->timers)
    {
        AstTimer *timer = (AstTimer *) lc_ptr(lc);

        analyze_timer(timer, state);
    }

    /* Phase 3: process remaining program clauses */
    foreach (lc, program->facts)
    {
        AstFact *fact = (AstFact *) lc_ptr(lc);

        /* Reset per-clause state */
        apr_hash_clear(state->var_tbl);
        apr_hash_clear(state->var_join_tbl);
        apr_hash_clear(state->eq_tbl);
        state->tmp_varno = 0;

        analyze_fact(fact, state);
    }

    foreach (lc, program->rules)
    {
        AstRule *rule = (AstRule *) lc_ptr(lc);

        /* Reset per-clause state */
        apr_hash_clear(state->var_tbl);
        apr_hash_clear(state->var_join_tbl);
        apr_hash_clear(state->eq_tbl);
        state->tmp_varno = 0;

        analyze_rule(rule, state);
    }
}
