#include <apr_hash.h>
#include <apr_strings.h>

#include "col-internal.h"
#include "parser/analyze.h"
#include "types/schema.h"

typedef struct AnalyzeState
{
    apr_pool_t *pool;
    AstProgram *program;
    apr_hash_t *define_tbl;
    apr_hash_t *rule_tbl;

    /* Current index for system-generated rule names */
    int tmp_ruleno;

    /* Per-rule state: */
    /* Table mapping var name => AstVarExpr */
    apr_hash_t *var_tbl;
    /* Current index for system-assigned variable names */
    int tmp_varno;
} AnalyzeState;


static void analyze_table_ref(AstTableRef *ref, AnalyzeState *state);
static void analyze_join_clause(AstJoinClause *join, AnalyzeState *state);
static void analyze_predicate(AstPredicate *pred, AnalyzeState *state);

static char *make_anon_rule_name(AnalyzeState *state);
static bool is_rule_defined(const char *name, AnalyzeState *state);
static bool is_var_defined(const char *name, AnalyzeState *state);
static void define_var(AstVarExpr *var, AnalyzeState *state);
static AstVarExpr *make_anon_var(const char *prefix, AnalyzeState *state);
static bool is_valid_type(const char *type_name);
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

        if (!is_valid_type(type_name))
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

    /* Reset per-rule state */
    apr_hash_clear(state->var_tbl);
    state->tmp_varno = 0;

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

    /* Check the rule body */
    foreach (lc, rule->body)
    {
        AstNode *node = (AstNode *) lc_ptr(lc);

        switch (node->kind)
        {
            case AST_JOIN_CLAUSE:
                analyze_join_clause((AstJoinClause *) node, state);
                break;

            case AST_PREDICATE:
                analyze_predicate((AstPredicate *) node, state);
                break;

            default:
                ERROR("Unrecognized node kind: %d", node->kind);
        }
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
    apr_hash_set(state->var_tbl, var->name,
                 APR_HASH_KEY_STRING, var);
}

static AstVarExpr *
make_anon_var(const char *prefix, AnalyzeState *state)
{
    AstVarExpr *var;
    char var_name[128];

    while (true)
    {
        snprintf(var_name, sizeof(var_name), "%s_%d",
                 prefix, state->tmp_varno++);

        if (!is_var_defined(var_name, state))
            break;
    }

    var = apr_pcalloc(state->pool, sizeof(*var));
    var->node.kind = AST_VAR_EXPR;
    var->name = apr_pstrdup(state->pool, var_name);
    return var;
}

static void
analyze_join_clause(AstJoinClause *join, AnalyzeState *state)
{
    AstDefine *define;
    ListCell *lc;

    define = apr_hash_get(state->define_tbl, join->ref->name,
                          APR_HASH_KEY_STRING);
    if (define == NULL)
        ERROR("No such table \"%s\"", join->ref->name);

    foreach (lc, join->ref->cols)
    {
        AstColumnRef *cref = (AstColumnRef *) lc_ptr(lc);

        /*
         * Rewrite constants in join clauses into an additional predicate "v
         * = k" on a variable with a system-generated name "v".
         */
        if (is_const_expr(cref->expr))
        {
            ;
        }
        else
        {
            AstVarExpr *var;

            ASSERT(cref->node.kind == AST_VAR_EXPR);
            var = (AstVarExpr *) cref->expr;

            /* Has this variable name already been used in this rule? */
            if (!is_var_defined(var->name, state))
            {
                define_var(var, state);
            }
            else
            {
                AstVarExpr *new_var;

                new_var = make_anon_var(var->name, state);
                cref->expr = (AstNode *) new_var;
                define_var(new_var, state);
#if 0
                add_eq_predicate();
#endif
            }
        }
    }
}

static void
analyze_predicate(AstPredicate *pred, AnalyzeState *state)
{
    ;
}

static void
analyze_fact(AstFact *fact, AnalyzeState *state)
{
    AstTableRef *target = fact->head;

    printf("FACT => %s\n", target->name);

    analyze_table_ref(target, state);
}

static void
analyze_table_ref(AstTableRef *ref, AnalyzeState *state)
{
    AstDefine *define;

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
}

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
is_valid_type(const char *type_name)
{
    if (strcmp(type_name, "bool") == 0)
        return true;
    if (strcmp(type_name, "char") == 0)
        return true;
    if (strcmp(type_name, "double") == 0)
        return true;
    if (strcmp(type_name, "int") == 0)
        return true;
    if (strcmp(type_name, "int2") == 0)
        return true;
    if (strcmp(type_name, "int4") == 0)
        return true;
    if (strcmp(type_name, "int8") == 0)
        return true;
    if (strcmp(type_name, "string") == 0)
        return true;

    return false;
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
    
}
