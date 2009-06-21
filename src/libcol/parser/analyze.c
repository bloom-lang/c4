#include <apr_hash.h>

#include "col-internal.h"
#include "parser/analyze.h"

typedef struct AnalyzeState
{
    apr_pool_t *pool;
    AstProgram *program;
    apr_hash_t *define_tbl;
} AnalyzeState;

static void analyze_table_ref(AstTableRef *ref, AnalyzeState *state);
static bool is_valid_type(const char *type_name);

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
        ERROR("Key list exceeds schema length for table %s",
              def->name);

    seen_keys = list_make(state->pool);
    foreach (lc, def->keys)
    {
        int key = lc_int(lc);

        if (key < 0)
            ERROR("Negative key %d in table %s", key, def->name);

        if (list_member_int(seen_keys, key))
            ERROR("Duplicate key %d for table %s", key, def->name);

        if (key >= list_length(def->schema))
            ERROR("Key index %d exceeds schema length for table %s",
                  key, def->name);

        list_append_int(seen_keys, key);
    }
}

static void
analyze_rule(AstRule *rule, AnalyzeState *state)
{
    ListCell *lc;

    printf("RULE => %s\n", rule->head->name);

    analyze_table_ref(rule->head, state);

    foreach (lc, rule->body)
    {
        AstNode *node = (AstNode *) lc_ptr(lc);

        switch (node->kind)
        {
            case AST_TABLE_REF:
                break;

            default:
                ERROR("Unrecognized node kind: %d", node->kind);
        }
    }
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
        ERROR("No such table \"%s\" in fact", ref->name);
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
    if (strcmp(type_name, "double") == 0)
        return true;
    if (strcmp(type_name, "int") == 0)
        return true;
    if (strcmp(type_name, "long") == 0)
        return true;
    if (strcmp(type_name, "string") == 0)
        return true;

    return false;
}
