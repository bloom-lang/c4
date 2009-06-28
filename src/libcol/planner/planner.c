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
#include "parser/copyfuncs.h"
#include "parser/parser.h"
#include "planner/planner.h"

typedef struct PlannerState
{
    ProgramPlan *plan;
    List *join_set_todo;
    List *qual_set_todo;
    List *join_set;
    List *qual_set;
    List *join_set_refs;

    apr_pool_t *plan_pool;
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
    state->plan_pool = plan_pool;
    state->tmp_pool = tmp_pool;
    state->plan = program_plan_make(ast, plan_pool);

    return state;
}

static void
split_rule_body(List *body, List **joins, List **quals, apr_pool_t *pool)
{
    ListCell *lc;

    foreach (lc, body)
    {
        AstNode *node = (AstNode *) lc_ptr(lc);

        node = node_deep_copy(node, pool);
        switch (node->kind)
        {
            case AST_JOIN_CLAUSE:
                list_append(*joins, node);
                break;

            case AST_QUALIFIER:
                list_append(*quals, node);
                break;

            default:
                ERROR("Unexpected node kind: %d", (int) node->kind);
        }
    }
}

static List *
make_join_set(AstJoinClause *delta_tbl, List *all_joins, PlannerState *state)
{
    List *result;
    ListCell *lc;
    bool seen_delta;

    result = list_make(state->tmp_pool);
    seen_delta = false;
    foreach (lc, all_joins)
    {
        AstJoinClause *join = (AstJoinClause *) lc_ptr(lc);

        if (join == delta_tbl && !seen_delta)
        {
            seen_delta = true;
            continue;
        }

        list_append(result, node_deep_copy(join, state->tmp_pool));
    }

    return result;
}

static bool
join_set_satisfies_qual(AstQualifier *qual, PlannerState *state)
{
    return false;
}

/*
 * Extract all the quals in "qual_set_todo" that can be applied to the
 * output of the join represented by "join_set".
 */
static void
extract_matching_quals(PlannerState *state, List **quals)
{
    ListCell *lc;

    foreach (lc, state->qual_set_todo)
    {
        AstQualifier *qual = (AstQualifier *) lc_ptr(lc);

        if (join_set_satisfies_qual(qual, state))
        {
            ;
        }
    }
}

static void
add_join_op(AstJoinClause *ast_join, List *quals,
            OpChain *op_chain, PlannerState *state)
{
    ;
}

static void
extend_op_chain(OpChain *op_chain, PlannerState *state)
{
    AstJoinClause *candidate;
    List *quals;

    /*
     * Choose a new relation from the TODO list to add to the join
     * set. Right now we just take the first element of the TODO list.
     */
    candidate = list_remove_head(state->join_set_todo);
    list_append(state->join_set, candidate);
    list_append(state->join_set_refs, candidate->ref);

    extract_matching_quals(state, &quals);
    add_join_op(candidate, quals, op_chain, state);
}

/*
 * Return an operator chain that begins with the given delta table. We need
 * to select the set of necessary operators and choose their order. We do
 * naive qualifier pushdown (qualifiers are associated with the first node
 * whose output contains all the variables in the qualifier); we utilize
 * indexes for qualifier evaluation in this way.
 */
static OpChain *
plan_op_chain(AstJoinClause *delta_tbl, AstRule *rule,
              RulePlan *rplan, PlannerState *state)
{
    OpChain *op_chain;
    ListCell *lc;

    state->join_set_todo = make_join_set(delta_tbl, rplan->ast_joins, state);
    state->qual_set_todo = list_deep_copy(rplan->ast_quals, state->tmp_pool);

    state->join_set = list_make1(delta_tbl, state->tmp_pool);
    state->join_set_refs = list_make1(delta_tbl->ref, state->tmp_pool);
    state->qual_set = list_make(state->tmp_pool);

    op_chain = apr_pcalloc(state->plan_pool, sizeof(*op_chain));
    op_chain->delta_tbl = node_deep_copy(delta_tbl, state->plan_pool);
    op_chain->chain_start = NULL;
    op_chain->length = 0;

    while (list_length(state->join_set_todo) > 0)
        extend_op_chain(op_chain, state);

    if (list_length(state->qual_set_todo) > 0)
        ERROR("Failed to match %d qualifiers to an operator",
              list_length(state->qual_set_todo));

    return op_chain;
}

static RulePlan *
plan_rule(AstRule *rule, PlannerState *state)
{
    RulePlan *rplan;
    ListCell *lc;

    rplan = apr_pcalloc(state->plan_pool, sizeof(*rplan));
    rplan->chains = list_make(state->plan_pool);
    rplan->ast_joins = list_make(state->plan_pool);
    rplan->ast_quals = list_make(state->plan_pool);

    split_rule_body(rule->body, &rplan->ast_joins,
                    &rplan->ast_quals, state->plan_pool);

    foreach (lc, rplan->ast_joins)
    {
        AstJoinClause *delta_tbl = (AstJoinClause *) lc_ptr(lc);
        OpChain *op_chain;

        op_chain = plan_op_chain(delta_tbl, rule, rplan, state);
        list_append(rplan->chains, op_chain);
    }

    return rplan;
}

ProgramPlan *
plan_program(AstProgram *ast, ColInstance *col)
{
    PlannerState *state;
    ProgramPlan *pplan;
    ListCell *lc;

    state = planner_state_make(ast, col);
    pplan = state->plan;

    /* XXX: Fill-in pplan->defines and pplan->facts */

    foreach (lc, ast->rules)
    {
        AstRule *rule = (AstRule *) lc_ptr(lc);
        RulePlan *rplan;

        rplan = plan_rule(rule, state);
        list_append(pplan->rules, rplan);
    }

    apr_pool_destroy(state->tmp_pool);
    return pplan;
}
