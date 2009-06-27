#include <apr_strings.h>

#include "col-internal.h"
#include "parser/parser.h"
#include "planner/planner.h"

typedef struct PlannerState
{
    ProgramPlan *plan;

    /*
     * Storage for temporary allocations made during the planning
     * process. This is a subpool of the plan pool; it is deleted when
     * planning completes.
     */
    apr_pool_t *tmp_pool;
} PlannerState;

static ProgramPlan *
program_plan_make(AstProgram *ast, apr_pool_t *pool)
{
    ProgramPlan *pplan;

    pplan = apr_pcalloc(pool, sizeof(*pplan));
    pplan->pool = pool;
    pplan->name = apr_pstrdup(pool, ast->name);
    /* XXX ... */

    return pplan;
}

static PlannerState *
planner_state_make(AstProgram *ast, ColInstance *col)
{
    PlannerState *state;
    apr_pool_t *plan_pool;
    apr_pool_t *tmp_pool;

    plan_pool = make_subpool(col->pool);
    tmp_pool = make_subpool(plan_pool);

    state = apr_pcalloc(tmp_pool, sizeof(*state));
    state->tmp_pool = tmp_pool;
    state->plan = program_plan_make(ast, plan_pool);

    return state;
}

ProgramPlan *
plan_program(AstProgram *ast, ColInstance *col)
{
    PlannerState *state;
    ProgramPlan *result;

    state = planner_state_make(ast, col);

    result = state->plan;
    apr_pool_destroy(state->tmp_pool);
    return result;
}

