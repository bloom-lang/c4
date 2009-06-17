#include <apr_hash.h>

#include "col-internal.h"
#include "parser/analyze.h"

typedef struct AnalyzeState
{
    apr_pool_t *pool;
    AstProgram *program;
    apr_hash_t *define_tbl;
} AnalyzeState;

static bool is_valid_type(const char *type_name);

static void
analyze_define(AstDefine *def, AnalyzeState *state)
{
    List *seen_keys;
    ListCell *lc;

    printf("DEFINE\n");

    /* Check for duplicate defines within the current program */
    if (apr_hash_get(state->define_tbl, def->name,
                     APR_HASH_KEY_STRING) == NULL)
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
    printf("RULE\n");
}

static void
analyze_fact(AstFact *fact, AnalyzeState *state)
{
    AstTableRef *target = fact->head;
    AstDefine *target_define;

    printf("FACT\n");

    if (target->hash_variant != AST_HASH_NONE)
        ERROR("Cannot specify \"#insert\" or \"#delete\" in a fact");

    /* Check that the specified table name exists */
    target_define = apr_hash_get(state->define_tbl, target->name,
                                 APR_HASH_KEY_STRING);
    if (target_define == NULL)
        ERROR("No such table \"%s\" in fact", target->name);
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

    foreach (lc, program->clauses)
    {
        AstClause *clause = (AstClause *) lc_ptr(lc);

        switch (clause->type)
        {
            case AST_DEFINE:
                analyze_define((AstDefine *) clause, state);
                break;

            case AST_RULE:
                analyze_rule((AstRule *) clause, state);
                break;

            case AST_FACT:
                analyze_fact((AstFact *) clause, state);
                break;

            default:
                ERROR("Unrecognized clause type: %d", clause->type);
        }
    }
}

static bool
is_valid_type(const char *type_name)
{
    if (strcmp(type_name, "int") == 0)
        return true;
    if (strcmp(type_name, "bool") == 0)
        return true;
    if (strcmp(type_name, "string") == 0)
        return true;
    if (strcmp(type_name, "double") == 0)
        return true;

    return false;
}
