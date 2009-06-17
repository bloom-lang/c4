#include "col-internal.h"
#include "parser/analyze.h"

static void
analyze_define(AstDefine *def, apr_pool_t *pool)
{
    ;
}

static void
analyze_rule(AstRule *rule, apr_pool_t *pool)
{
    ;
}

static void
analyze_fact(AstFact *fact, apr_pool_t *pool)
{
    ;
}

void
analyze_ast(AstProgram *program, apr_pool_t *pool)
{
    ListCell *lc;

    foreach (lc, program->clauses)
    {
        AstClause *clause = (AstClause *) lc->data;

        switch (clause->type)
        {
            case AST_DEFINE:
                analyze_define((AstDefine *) clause, pool);
                break;

            case AST_RULE:
                analyze_rule((AstRule *) clause, pool);
                break;

            case AST_FACT:
                analyze_fact((AstFact *) clause, pool);
                break;

            default:
                ERROR("Unrecognized clause type: %d", clause->type);
        }
    }
}
