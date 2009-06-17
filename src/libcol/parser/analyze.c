#include <apr_hash.h>

#include "col-internal.h"
#include "parser/analyze.h"

typedef struct AnalyzeState
{
    apr_pool_t *pool;
    AstProgram *program;
    apr_hash_t *define_tbl;
} AnalyzeState;

static void
analyze_define(AstDefine *def, AnalyzeState *state)
{
    printf("DEFINE\n");

    /* Check for duplicate defines within the current program */
    if (apr_hash_get(state->define_tbl, def->name,
                     APR_HASH_KEY_STRING) == NULL)
        ERROR("Duplicate table name \"%s\" in program %s",
              def->name, state->program->name);

    apr_hash_set(state->define_tbl, def->name,
                 APR_HASH_KEY_STRING, def);
}

static void
analyze_rule(AstRule *rule, AnalyzeState *state)
{
    printf("RULE\n");
}

static void
analyze_fact(AstFact *fact, AnalyzeState *state)
{
    printf("FACT\n");
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
        AstClause *clause = (AstClause *) lc->data;

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
