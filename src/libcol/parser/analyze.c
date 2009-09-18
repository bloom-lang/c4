#include <apr_hash.h>

#include "col-internal.h"
#include "nodes/makefuncs.h"
#include "parser/analyze.h"
#include "types/catalog.h"

typedef struct AnalyzeState
{
    apr_pool_t *pool;
    AstProgram *program;
    apr_hash_t *define_tbl;
    apr_hash_t *rule_tbl;

    /* Current index for system-generated rule names */
    int tmp_ruleno;

    /* Per-clause state: */
    /* Map from variable name => AstVarExpr */
    apr_hash_t *var_tbl;
    /* Map from variable name => list of equal variable names */
    apr_hash_t *eq_tbl;
    /* Current index for system-generated variable names */
    int tmp_varno;
} AnalyzeState;


static void analyze_table_ref(AstTableRef *ref, AnalyzeState *state);
static void analyze_join_clause(AstJoinClause *join, AstRule *rule,
                                AnalyzeState *state);
static void analyze_qualifier(AstQualifier *qual, AnalyzeState *state);
static void analyze_expr(ColNode *node, bool inside_qual, AnalyzeState *state);
static void analyze_op_expr(AstOpExpr *op_expr, bool inside_qual,
                            AnalyzeState *state);
static void analyze_var_expr(AstVarExpr *var_expr, bool inside_qual,
                             AnalyzeState *state);
static void analyze_const_expr(AstConstExpr *c_expr, AnalyzeState *state);

static AstColumnRef *table_ref_get_loc_spec(AstTableRef *ref,
                                            AnalyzeState *state);
static bool loc_spec_equal(AstColumnRef *s1, AstColumnRef *s2,
                           AnalyzeState *state);
static char *make_anon_rule_name(AnalyzeState *state);
static bool is_rule_defined(const char *name, AnalyzeState *state);
static bool is_var_defined(const char *name, AnalyzeState *state);
static AstVarExpr *lookup_var(const char *name, AnalyzeState *state);
static void define_var(AstVarExpr *var, AnalyzeState *state);
static bool is_var_equal(AstVarExpr *v1, AstVarExpr *v2, AnalyzeState *state);
static char *make_anon_var_name(const char *prefix, AnalyzeState *state);
static void set_var_type(AstVarExpr *var, AstDefine *table, int colno);
static void add_qual(char *lhs_name, ColNode *rhs, AstOperKind op_kind,
                     AstRule *rule, AnalyzeState *state);
static bool is_dont_care_var(ColNode *node);
static void make_var_eq_table(AstRule *rule, AnalyzeState *state);
static void generate_implied_quals(AstRule *rule, AnalyzeState *state);
static List *get_eq_list(const char *varname, bool make_new, AnalyzeState *state);

static DataType op_expr_get_type(AstOpExpr *op_expr);
static DataType const_expr_get_type(AstConstExpr *c_expr);
static DataType var_expr_get_type(AstVarExpr *var);


static void
analyze_define(AstDefine *def, AnalyzeState *state)
{
    List *seen_keys;
    bool seen_loc_spec;
    ListCell *lc;

    printf("DEFINE => %s\n", def->name);

    /* Check for duplicate defines within the current program */
    if (apr_hash_get(state->define_tbl, def->name,
                     APR_HASH_KEY_STRING) != NULL)
        ERROR("Duplicate table name %s in program %s",
              def->name, state->program->name);

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
analyze_rule(AstRule *rule, AnalyzeState *state)
{
    AstColumnRef *body_loc_spec;
    AstColumnRef *head_loc_spec;
    ListCell *lc;

    printf("RULE => %s\n", rule->head->name);

    /*
     * If the rule has a user-defined name, check that it is unique within
     * the current program. If no name was provided, generate a unique name.
     */
    if (rule->name)
    {
        if (is_rule_defined(rule->name, state))
            ERROR("Duplicate rule name: %s", rule->name);
    }
    else
    {
        rule->name = make_anon_rule_name(state);
    }

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

    /* Check the rule head */
    analyze_table_ref(rule->head, state);
    foreach (lc, rule->head->cols)
    {
        AstColumnRef *col = (AstColumnRef *) lc_ptr(lc);

        if (is_dont_care_var(col->expr))
            ERROR("Don't care variables (\"_\") cannot "
                  "appear in the head of a rule");
    }

    make_var_eq_table(rule, state);
    generate_implied_quals(rule, state);

    /*
     * Check the consistency of the location specifiers. At most one
     * distinct location specifier can be used in the rule body.
     */
    body_loc_spec = NULL;
    foreach (lc, rule->joins)
    {
        AstJoinClause *join = (AstJoinClause *) lc_ptr(lc);
        AstColumnRef *loc_spec;

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

static int
table_get_loc_spec_colno(const char *tbl_name, AnalyzeState *state)
{
    AstDefine *define;
    int colno;
    ListCell *lc;

    define = apr_hash_get(state->define_tbl, tbl_name,
                          APR_HASH_KEY_STRING);

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

static AstColumnRef *
table_ref_get_loc_spec(AstTableRef *ref, AnalyzeState *state)
{
    int colno;

    colno = table_get_loc_spec_colno(ref->name, state);
    if (colno == -1)
        return NULL;

    return (AstColumnRef *) list_get(ref->cols, colno);
}

static bool
loc_spec_equal(AstColumnRef *s1, AstColumnRef *s2, AnalyzeState *state)
{
    /* Only variable location specifiers supported for now */
    ASSERT(s1->expr->kind == AST_VAR_EXPR);
    ASSERT(s2->expr->kind == AST_VAR_EXPR);

    /*
     * NB: This only checks if there is an explicit or implied equality
     * constraint between the two variable names. In practice, this is
     * probably sufficient.
     */
    return is_var_equal((AstVarExpr *) s1->expr, (AstVarExpr *) s2->expr,
                        state);
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
define_var(AstVarExpr *var, AnalyzeState *state)
{
    ASSERT(!is_dont_care_var((ColNode *) var));
    ASSERT(!is_var_defined(var->name, state));
    apr_hash_set(state->var_tbl, var->name,
                 APR_HASH_KEY_STRING, var);
}

static bool
is_var_equal(AstVarExpr *v1, AstVarExpr *v2, AnalyzeState *state)
{
    List *eq_list;

    eq_list = get_eq_list(v1->name, false, state);
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
    AstDefine *define;
    int colno;
    ListCell *lc;

    define = apr_hash_get(state->define_tbl, join->ref->name,
                          APR_HASH_KEY_STRING);
    if (define == NULL)
        ERROR("No such table \"%s\"", join->ref->name);

    colno = 0;
    foreach (lc, join->ref->cols)
    {
        AstColumnRef *cref = (AstColumnRef *) lc_ptr(lc);
        AstVarExpr *var;

        /*
         * Replace constants in join clauses with a system-generated
         * variable, and then add an equality qualifier between the new
         * variable and the constant value.
         */
        if (cref->expr->kind == AST_CONST_EXPR)
        {
            ColNode *const_expr = cref->expr;
            char *var_name;

            var_name = make_anon_var_name("Const", state);
            /* Type will be filled-in shortly */
            var = make_ast_var_expr(var_name, TYPE_INVALID, state->pool);
            cref->expr = (ColNode *) var;

            add_qual(var_name, const_expr, AST_OP_EQ, rule, state);
        }
        else
        {
            if (is_dont_care_var(cref->expr))
                goto done;

            ASSERT(cref->expr->kind == AST_VAR_EXPR);
            var = (AstVarExpr *) cref->expr;

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
                add_qual(old_name, (ColNode *) var,
                         AST_OP_EQ, rule, state);
            }
        }

done:
        /*
         * We are left with either a uniquely-occurring variable or a don't
         * care variable.
         */
        ASSERT(cref->expr->kind == AST_VAR_EXPR);
        var = (AstVarExpr *) cref->expr;

        /* Fill-in variable's type, add to variable table (unless don't care) */
        set_var_type(var, define, colno);
        if (!is_dont_care_var((ColNode *) var))
            define_var(var, state);
        colno++;
    }

    analyze_table_ref(join->ref, state);
}

static void
set_var_type(AstVarExpr *var, AstDefine *table, int colno)
{
    AstSchemaElt *elt;

    elt = (AstSchemaElt *) list_get(table->schema, colno);
    var->type = get_type_id(elt->type_name);
}

static void
add_qual(char *lhs_name, ColNode *rhs, AstOperKind op_kind,
         AstRule *rule, AnalyzeState *state)
{
    AstVarExpr *lhs;
    AstOpExpr *op_expr;
    AstQualifier *qual;

    lhs = make_ast_var_expr(lhs_name, TYPE_INVALID, state->pool);
    op_expr = make_ast_op_expr((ColNode *) lhs, rhs,
                               op_kind, state->pool);
    qual = make_qualifier((ColNode *) op_expr, state->pool);

    list_append(rule->quals, qual);
}

static void
analyze_qualifier(AstQualifier *qual, AnalyzeState *state)
{
    DataType type;

    analyze_expr(qual->expr, true, state);

    type = expr_get_type(qual->expr);
    if (type != TYPE_BOOL)
        ERROR("Qualifier must have boolean result, not %s",
              get_type_name(type));
}

static void
analyze_expr(ColNode *node, bool inside_qual, AnalyzeState *state)
{
    switch (node->kind)
    {
        case AST_OP_EXPR:
            analyze_op_expr((AstOpExpr *) node, inside_qual, state);
            break;

        case AST_VAR_EXPR:
            analyze_var_expr((AstVarExpr *) node, inside_qual, state);
            break;

        case AST_CONST_EXPR:
            analyze_const_expr((AstConstExpr *) node, state);
            break;

        default:
            ERROR("Unexpected expr node kind: %d", (int) node->kind);
    }
}

static void
analyze_op_expr(AstOpExpr *op_expr, bool inside_qual, AnalyzeState *state)
{
    DataType lhs_type;
    DataType rhs_type;

    analyze_expr(op_expr->lhs, inside_qual, state);
    lhs_type = expr_get_type(op_expr->lhs);

    /* Unary operators are simple */
    if (op_expr->op_kind == AST_OP_UMINUS)
    {
        if (!is_numeric_type(lhs_type))
            ERROR("Unary minus must have numeric input");

        return;
    }

    analyze_expr(op_expr->rhs, inside_qual, state);
    rhs_type = expr_get_type(op_expr->rhs);

    /* XXX: type compatibility check is far too strict */
    if (lhs_type != rhs_type)
        ERROR("Type mismatch in qualifier: lhs is %s, rhs is %s",
              get_type_name(lhs_type), get_type_name(rhs_type));

    switch (op_expr->op_kind)
    {
        case AST_OP_PLUS:
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
analyze_var_expr(AstVarExpr *var_expr, bool inside_qual, AnalyzeState *state)
{
    if (is_dont_care_var((ColNode *) var_expr))
    {
        if (inside_qual)
            ERROR("Don't care variables (\"_\") cannot be "
                  "referenced in qualifiers");

        /*
         * If we're not inside a qual, we're done: we don't want to lookup
         * the variable's name, and don't care variables in join clauses or
         * rule heads have already had their type computed.
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
analyze_fact(AstFact *fact, AnalyzeState *state)
{
    AstTableRef *target = fact->head;
    ListCell *lc;

    printf("FACT => %s\n", target->name);

    analyze_table_ref(target, state);

    foreach (lc, target->cols)
    {
        AstColumnRef *col = (AstColumnRef *) lc_ptr(lc);

        if (col->expr->kind != AST_CONST_EXPR)
            ERROR("Columns in a fact must be constants");
    }
}

static void
analyze_table_ref(AstTableRef *ref, AnalyzeState *state)
{
    AstDefine *define;
    ListCell *lc;
    int colno;

    /* Check that the specified table name exists */
    define = apr_hash_get(state->define_tbl, ref->name,
                          APR_HASH_KEY_STRING);
    if (define == NULL)
        ERROR("No such table \"%s\"", ref->name);

    /*
     * Check that the data types of the table reference are compatible with
     * the schema of the table
     */
    if (list_length(define->schema) != list_length(ref->cols))
        ERROR("Reference to table %s has incorrect # of columns. "
              "Expected: %d, encountered %d",
              ref->name, list_length(define->schema),
              list_length(ref->cols));

    colno = 0;
    foreach (lc, ref->cols)
    {
        AstColumnRef *col = (AstColumnRef *) lc_ptr(lc);
        DataType expr_type;
        DataType schema_type;
        AstSchemaElt *schema_elt;

        analyze_expr(col->expr, false, state);
        expr_type = expr_get_type(col->expr);
        schema_elt = (AstSchemaElt *) list_get(define->schema, colno);
        schema_type = get_type_id(schema_elt->type_name);

        /* XXX: type compatibility check is far too strict */
        if (schema_type != expr_type)
            ERROR("Type mismatch in column list: %s in schema, %s in column list",
                  schema_elt->type_name, get_type_name(expr_type));

        colno++;
    }
}

/*
 * Invoke the semantic analyzer on the specified program. Note that the
 * analysis phase is side-effecting: the input AstProgram is destructively
 * modified.
 */
void
analyze_ast(AstProgram *program, apr_pool_t *pool)
{
    AnalyzeState *state;
    ListCell *lc;

    state = apr_palloc(pool, sizeof(*state));
    state->pool = pool;
    state->program = program;
    state->define_tbl = apr_hash_make(pool);
    state->rule_tbl = apr_hash_make(pool);
    state->var_tbl = apr_hash_make(pool);
    state->eq_tbl = apr_hash_make(pool);
    state->tmp_ruleno = 0;

    printf("Program name: %s\n", program->name);

    /* Phase 1: process table definitions */
    foreach (lc, program->defines)
    {
        AstDefine *def = (AstDefine *) lc_ptr(lc);

        analyze_define(def, state);
    }

    /* Phase 2: process remaining program clauses */
    foreach (lc, program->facts)
    {
        AstFact *fact = (AstFact *) lc_ptr(lc);

        /* Reset per-clause state */
        apr_hash_clear(state->var_tbl);
        apr_hash_clear(state->eq_tbl);
        state->tmp_varno = 0;

        analyze_fact(fact, state);
    }

    foreach (lc, program->rules)
    {
        AstRule *rule = (AstRule *) lc_ptr(lc);

        /* Reset per-clause state */
        apr_hash_clear(state->var_tbl);
        apr_hash_clear(state->eq_tbl);
        state->tmp_varno = 0;

        analyze_rule(rule, state);
    }
}

static bool
is_dont_care_var(ColNode *node)
{
    AstVarExpr *var;

    if (node->kind != AST_VAR_EXPR)
        return false;

    var = (AstVarExpr *) node;
    return (bool) (strcmp(var->name, "_") == 0);
}

static List *
get_eq_list(const char *varname, bool make_new, AnalyzeState *state)
{
    List *eq_list;

    eq_list = apr_hash_get(state->eq_tbl, varname, APR_HASH_KEY_STRING);
    if (eq_list == NULL)
    {
        if (!make_new)
            ERROR("Failed to find equality list for var %s", varname);

        eq_list = list_make(state->pool);
        apr_hash_set(state->eq_tbl, varname, APR_HASH_KEY_STRING, eq_list);
    }

    return eq_list;
}

/* Record that "v2" is an alias for "v1". */
static void
add_var_eq(char *v1, char *v2, AnalyzeState *state)
{
    List *eq_list;

    eq_list = get_eq_list(v1, true, state);
    list_append(eq_list, v2);
}

/*
 * We use a simple definition of "equality": we look for an op expression
 * consisting of "=" between two bare variables.
 */
static bool
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
    ListCell *lc;

    ASSERT(apr_hash_count(state->eq_tbl) == 0);

    foreach (lc, rule->quals)
    {
        AstQualifier *qual = (AstQualifier *) lc_ptr(lc);
        AstVarExpr *lhs;
        AstVarExpr *rhs;

        if (!is_simple_equality(qual, &lhs, &rhs))
            continue;

        add_var_eq(lhs->name, rhs->name, state);
        add_var_eq(rhs->name, lhs->name, state);
    }
}

static int
add_qual_list(AstVarExpr *target, List *var_list,
              AstRule *rule, AnalyzeState *state)
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

        eq_list = get_eq_list(var, false, state);
        if (!list_member_str(eq_list, target->name))
        {
            /* Create the actual qual */
            AstVarExpr *rhs;
            AstOpExpr *op_expr;
            AstQualifier *qual;

            rhs = lookup_var(var, state);
            op_expr = make_ast_op_expr((ColNode *) target,
                                       (ColNode *) rhs,
                                       AST_OP_EQ,
                                       state->pool);
            qual = make_qualifier((ColNode *) op_expr, state->pool);
            list_append(rule->quals, qual);

            add_var_eq(var, target->name, state);
            add_var_eq(target->name, var, state);
            list_append(eq_list, target->name);
            count++;
        }
    }

    return count;
}

/*
 * Compute qualifiers that are implied by the set of quals explicitly stated
 * in the query. We only try to do this for the trivial case of computing
 * the transitive closure over the simple var-equality quals: that is, given
 * A = B and B = C, we generate A = C.
 *
 * This is useful, because it gives the planner more choice for join
 * options. It is also necessary for counting the number of distinct
 * location specifiers in the rule.
 *
 * XXX: We currently do this via a braindead algorithm.
 */
static void
generate_implied_quals(AstRule *rule, AnalyzeState *state)
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

        eq_list = get_eq_list(lhs->name, false, state);
        count += add_qual_list(rhs, eq_list, rule, state);

        eq_list = get_eq_list(rhs->name, false, state);
        count += add_qual_list(lhs, eq_list, rule, state);
    }

    if (count > 0)
        goto top;
}

DataType
expr_get_type(ColNode *node)
{
    switch (node->kind)
    {
        case AST_OP_EXPR:
            return op_expr_get_type((AstOpExpr *) node);

        case AST_VAR_EXPR:
            return var_expr_get_type((AstVarExpr *) node);

        case AST_CONST_EXPR:
            return const_expr_get_type((AstConstExpr *) node);

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
