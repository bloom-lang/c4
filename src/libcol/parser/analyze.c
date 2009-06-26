#include <apr_hash.h>
#include <apr_strings.h>

#include "col-internal.h"
#include "parser/analyze.h"
#include "parser/makefuncs.h"
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
    /* Mapping from variable name => AstVarExpr */
    apr_hash_t *var_tbl;
    /* Current index for system-generated variable names */
    int tmp_varno;
} AnalyzeState;


static void analyze_table_ref(AstTableRef *ref, AnalyzeState *state);
static void analyze_join_clause(AstJoinClause *join, AnalyzeState *state);
static void analyze_predicate(AstPredicate *pred, AnalyzeState *state);
static void analyze_expr(AstNode *node, AnalyzeState *state);

static char *make_anon_rule_name(AnalyzeState *state);
static bool is_rule_defined(const char *name, AnalyzeState *state);
static bool is_var_defined(const char *name, AnalyzeState *state);
static void define_var(AstVarExpr *var, AnalyzeState *state);
static char *make_anon_var_name(const char *prefix, AnalyzeState *state);
static void set_var_type(AstVarExpr *var, AstDefine *table,
                         int colno, AnalyzeState *state);
static void add_predicate(char *lhs_name, AstNode *rhs, AstOperKind op_kind,
                          AnalyzeState *state);
static bool is_dont_care_var(AstNode *node);
static bool is_const_expr(AstNode *node);
static DataType expr_get_type(AstNode *node, AnalyzeState *state);
static DataType op_expr_get_type(AstOpExpr *op_expr, AnalyzeState *state);
static DataType const_expr_get_type(AstNode *node, AnalyzeState *state);
static DataType var_expr_get_type(AstVarExpr *var, AnalyzeState *state);


static void
analyze_define(AstDefine *def, AnalyzeState *state)
{
    List *seen_keys;
    ListCell *lc;

    printf("DEFINE => %s\n", def->name);

    /* Check for duplicate defines within the current program */
    if (apr_hash_get(state->define_tbl, def->name,
                     APR_HASH_KEY_STRING) != NULL)
        ERROR("Duplicate table name %s in program %s",
              def->name, state->program->name);

    apr_hash_set(state->define_tbl, def->name,
                 APR_HASH_KEY_STRING, def);

    /* Validate the schema list */
    foreach (lc, def->schema)
    {
        char *type_name = (char *) lc_ptr(lc);

        if (!is_valid_type_name(type_name))
            ERROR("Undefined type %s in schema of table %s",
                  type_name, def->name);
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
     * examine the predicates (all join variables are "in-scope" for
     * predicates, even if they appear before the join in the program text).
     */
    foreach (lc, rule->body)
    {
        AstNode *node = (AstNode *) lc_ptr(lc);

        if (node->kind == AST_JOIN_CLAUSE)
            analyze_join_clause((AstJoinClause *) node, state);
    }

    foreach (lc, rule->body)
    {
        AstNode *node = (AstNode *) lc_ptr(lc);

        if (node->kind == AST_PREDICATE)
            analyze_predicate((AstPredicate *) node, state);
    }

    /* Check the rule head */
    analyze_table_ref(rule->head, state);
    foreach (lc, rule->head->cols)
    {
        AstColumnRef *col = (AstColumnRef *) lc_ptr(lc);

        if (is_dont_care_var(col->expr))
            ERROR("Don't care variables (\"_\") cannot appear in rule heads");
    }
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
    return (bool) (apr_hash_get(state->var_tbl, name,
                                APR_HASH_KEY_STRING) != NULL);
}

static void
define_var(AstVarExpr *var, AnalyzeState *state)
{
    ASSERT(!is_var_defined(var->name, state));
    apr_hash_set(state->var_tbl, var->name,
                 APR_HASH_KEY_STRING, var);
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
analyze_join_clause(AstJoinClause *join, AnalyzeState *state)
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
         * variable, and then add an equality predicate between the new
         * variable and the constant value.
         */
        if (is_const_expr(cref->expr))
        {
            AstNode *const_expr = cref->expr;
            char *var_name;

            var_name = make_anon_var_name("const", state);
            var = make_var_expr(var_name, state->pool);
            cref->expr = (AstNode *) var;

            add_predicate(var_name, const_expr, AST_OP_EQ, state);
        }
        else
        {
            ASSERT(cref->node.kind == AST_VAR_EXPR);
            var = (AstVarExpr *) cref->expr;

            /*
             * Check if the variable name has already been used by a
             * JoinClause in this rule.  If it has, then assign a new
             * system-generated name to the variable, and add an equality
             * predicate between the new and old variable names.
             */
            if (is_var_defined(var->name, state))
            {
                char *old_name = var->name;

                var->name = make_anon_var_name(var->name, state);
                add_predicate(old_name, (AstNode *) var, AST_OP_EQ, state);
            }
        }

        /* We should now be left with a uniquely-occurring variable */
        ASSERT(cref->expr->kind == AST_VAR_EXPR);
        var = (AstVarExpr *) cref->expr;

        /* Fill-in variable's type, add to variable table */
        set_var_type((AstVarExpr *) cref->expr, define, colno, state);
        define_var(var, state);
        colno++;
    }
}

static void
set_var_type(AstVarExpr *var, AstDefine *table, int colno, AnalyzeState *state)
{
    char *type_name;

    type_name = list_get(table->schema, colno);
    var->type = get_type_id(type_name);
}

static void
add_predicate(char *lhs_name, AstNode *rhs,
              AstOperKind op_kind, AnalyzeState *state)
{
    AstVarExpr *lhs;
    AstOpExpr *op_expr;
    AstPredicate *pred;

    lhs = make_var_expr(lhs_name, state->pool);
    op_expr = make_op_expr((AstNode *) lhs, rhs,
                           op_kind, state->pool);
    pred = make_predicate((AstNode *) op_expr, state->pool);

    list_append(state->program->clauses, pred);
}

static void
analyze_predicate(AstPredicate *pred, AnalyzeState *state)
{
    analyze_expr(pred->expr, state);
}

static void
analyze_expr(AstNode *node, AnalyzeState *state)
{
    switch (node->kind)
    {
        case AST_OP_EXPR:
        {
            AstOpExpr *op_expr = (AstOpExpr *) node;
            break;
        }

        case AST_VAR_EXPR:
            break;

        default:
            if (is_const_expr(node))
                ;
            break;
    }
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

        if (col->has_loc_spec)
            ERROR("Columns in a fact cannot have location specifiers");

        if (!is_const_expr(col->expr))
            ERROR("Columns in a fact must be constants");
    }
}

static void
analyze_table_ref(AstTableRef *ref, AnalyzeState *state)
{
    AstDefine *define;
    ListCell *lc;
    int colno;
    int loc_spec_count;

    /* Check that the specified table name exists */
    define = apr_hash_get(state->define_tbl, ref->name,
                          APR_HASH_KEY_STRING);
    if (define == NULL)
        ERROR("No such table \"%s\"", ref->name);

    /*
     * Check that the reference data types are compatible with the table
     * schema
     */
    if (list_length(define->schema) != list_length(ref->cols))
        ERROR("Reference to table %s has incorrect # of columns. "
              "Expected: %d, encountered %d",
              ref->name, list_length(define->schema),
              list_length(ref->cols));

    colno = 0;
    loc_spec_count = 0;
    foreach (lc, ref->cols)
    {
        AstColumnRef *col = (AstColumnRef *) lc_ptr(lc);
        DataType expr_type;
        DataType schema_type;
        char *schema_type_name;

        expr_type = expr_get_type(col->expr, state);
        if (col->has_loc_spec)
        {
            if (expr_type != TYPE_STRING)
                ERROR("Location specifiers must be of type \"string\"");

            loc_spec_count++;
            if (loc_spec_count > 1)
                ERROR("Multiple location specifiers in a "
                      "single clause are not allowed");
        }

        schema_type_name = (char *) list_get(define->schema, colno);
        schema_type = get_type_id(schema_type_name);

        /* XXX: type compatibility check is far too strict */
        if (schema_type != expr_type)
            ERROR("Type mismatch in column list: %s in schema, %s in column list",
                  schema_type_name, get_type_name(expr_type));
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
    state->tmp_ruleno = 0;

    printf("Program name: %s; # of clauses: %d\n",
           program->name, list_length(program->clauses));

    /* Phase 1: process table definitions */
    foreach (lc, program->clauses)
    {
        AstNode *node = (AstNode *) lc_ptr(lc);

        if (node->kind == AST_DEFINE)
            analyze_define((AstDefine *) node, state);
    }

    /* Phase 2: process remaining program clauses */
    foreach (lc, program->clauses)
    {
        AstNode *node = (AstNode *) lc_ptr(lc);

        /* Reset per-clause state */
        apr_hash_clear(state->var_tbl);
        state->tmp_varno = 0;

        switch (node->kind)
        {
            /* Skip in phase 2, already processed */
            case AST_DEFINE:
                break;

            case AST_RULE:
                analyze_rule((AstRule *) node, state);
                break;

            case AST_FACT:
                analyze_fact((AstFact *) node, state);
                break;

            default:
                ERROR("Unrecognized node kind: %d", node->kind);
        }
    }
}

static bool
is_dont_care_var(AstNode *node)
{
    AstVarExpr *var;

    if (node->kind != AST_VAR_EXPR)
        return false;

    var = (AstVarExpr *) node;
    return (bool) (strcmp(var->name, "_") == 0);
}

static DataType
expr_get_type(AstNode *node, AnalyzeState *state)
{
    switch (node->kind)
    {
        case AST_OP_EXPR:
            return op_expr_get_type((AstOpExpr *) node, state);

        case AST_VAR_EXPR:
            return var_expr_get_type((AstVarExpr *) node, state);

        default:
            if (is_const_expr(node))
                return const_expr_get_type(node, state);

            ERROR("unexpected node kind: %d", (int) node->kind);
            return 0;   /* keep compiler quiet */
    }
}

static DataType
op_expr_get_type(AstOpExpr *op_expr, AnalyzeState *state)
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
            return expr_get_type(op_expr->lhs, state);

        case AST_OP_PLUS:
        case AST_OP_MINUS:
        case AST_OP_TIMES:
        case AST_OP_DIVIDE:
        case AST_OP_MODULUS:
        {
            DataType lhs_type = expr_get_type(op_expr->lhs, state);
            DataType rhs_type = expr_get_type(op_expr->rhs, state);

            /* XXX: probably too strict; need to allow implicit casts */
            ASSERT(lhs_type == rhs_type);
            return lhs_type;
        }

        case AST_OP_ASSIGN:
        default:
            FAIL();
            return 0;   /* keep compiler quiet */
    }
}

static bool
is_const_expr(AstNode *node)
{
    switch (node->kind)
    {
        case AST_CONST_EXPR_BOOL:
        case AST_CONST_EXPR_CHAR:
        case AST_CONST_EXPR_DOUBLE:
        case AST_CONST_EXPR_INT:
        case AST_CONST_EXPR_STRING:
            return true;

        default:
            return false;
    }
}

static DataType
const_expr_get_type(AstNode *node, AnalyzeState *state)
{
    switch (node->kind)
    {
        case AST_CONST_EXPR_BOOL:
            return TYPE_BOOL;

        case AST_CONST_EXPR_CHAR:
            return TYPE_CHAR;

        case AST_CONST_EXPR_DOUBLE:
            return TYPE_DOUBLE;

        case AST_CONST_EXPR_INT:
            return TYPE_INT8;

        case AST_CONST_EXPR_STRING:
            return TYPE_STRING;

        default:
            ERROR("unexpected node kind: %d", (int) node->kind);
            return 0;   /* keep compiler quiet */
    }
}

static DataType
var_expr_get_type(AstVarExpr *var, AnalyzeState *state)
{
    ASSERT(var->type != TYPE_INVALID);
    return var->type;
}
