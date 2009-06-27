/*
 * Basic planner algorithm is described below. Doesn't handle negation,
 * aggregation, stratification, intelligent selection of join order; no
 * significant optimization performed.
 *
 * Foreach rule r:
 *  Foreach join clause j in r->body:
 *    Make_op_chain(j, r->body - j, r->head);
 *
 * Make_op_chain(delta, body, head):
 *  Divide body into join clauses J and qualifiers Q
 *  Done = {delta}
 *  Chain = []
 *  while J != {}:
 *    Select an element j from J which joins against a relation in Done with qualifier q
 *    Remove j from J, remove q from Q
 *    Done = Done U j
 *    Chain << Make_Op(j, q)
 *
 *  { Handle additional qualifiers }
 *  while Q != {}:
 *    Remove q from Q
 *    Push q down Chain until the first place in the Chain where all the
 *    tables referenced in q have been joined against
 */
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
    pplan->defines = list_make(pool);
    pplan->facts = list_make(pool);
    pplan->rules = list_make(pool);

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

static void
plan_rule(AstRule *rule, PlannerState *state)
{
    ;
}

static void
split_program_clauses(List *clauses, List **defines,
                      List **facts, List **rules)
{
    ListCell *lc;

    foreach (lc, clauses)
    {
        AstNode *node = (AstNode *) lc_ptr(lc);

        switch (node->kind)
        {
            case AST_DEFINE:
                list_append(*defines, node);
                break;

            case AST_FACT:
                list_append(*facts, node);
                break;

            case AST_RULE:
                list_append(*rules, node);
                break;

            default:
                ERROR("Unexpected node kind: %d", (int) node->kind);
        }
    }
}

ProgramPlan *
plan_program(AstProgram *ast, ColInstance *col)
{
    PlannerState *state;
    ProgramPlan *plan;
    ListCell *lc;

    state = planner_state_make(ast, col);
    plan = state->plan;

    split_program_clauses(ast->clauses, &plan->defines,
                          &plan->facts, &plan->rules);

    foreach (lc, plan->rules)
    {
        AstRule *rule = (AstRule *) lc_ptr(lc);

        plan_rule(rule, state);
    }

    apr_pool_destroy(state->tmp_pool);
    return plan;
}

