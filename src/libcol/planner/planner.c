/*
 * Basic planner algorithm is described below. Doesn't handle negation,
 * aggregation, stratification, or intelligent selection of join order; no
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
#include "parser/walker.h"
#include "planner/planner.h"

typedef struct PlannerState
{
    ProgramPlan *plan;
    List *join_set_todo;
    List *qual_set_todo;
    List *join_set;
    List *qual_set;
    List *join_set_refs;

    /* The pool in which the output plan is allocated */
    apr_pool_t *plan_pool;
    /*
     * Storage for temporary allocations made during the planning
     * process. This is a subpool of the plan pool; it is deleted when
     * planning is complete.
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

static List *
make_join_set(AstJoinClause *delta_tbl, List *all_joins, PlannerState *state)
{
    List *result;
    ListCell *lc;

    result = list_make(state->tmp_pool);
    foreach (lc, all_joins)
    {
        AstJoinClause *join = (AstJoinClause *) lc_ptr(lc);

        if (join == delta_tbl)
            continue;

        list_append(result, join);
    }

    ASSERT(list_length(result) + 1 == list_length(all_joins));
    return result;
}

/*
 * A variable is satisfied by the current join set if it appears in a column
 * of one of the joins. XXX: We should also check for variable equivalences.
 */
static bool
join_set_satisfies_var(AstVarExpr *var, PlannerState *state)
{
    ListCell *lc;

    foreach (lc, state->join_set)
    {
        AstJoinClause *join = (AstJoinClause *) lc_ptr(lc);
        ListCell *lc2;

        foreach (lc2, join->ref->cols)
        {
            AstColumnRef *cref = (AstColumnRef *) lc_ptr(lc2);
            AstVarExpr *column_var;

            /* XXX: we shouldn't actually see this post-analysis, right? */
            if (cref->expr->kind != AST_VAR_EXPR)
                continue;

            column_var = (AstVarExpr *) cref->expr;
            if (strcmp(var->name, column_var->name) == 0)
                return true;
        }
    }

    return false;
}

static bool
get_var_callback(AstNode *n, void *data)
{
    List *var_list = (List *) data;

    if (n->kind == AST_VAR_EXPR)
        list_append(var_list, n);

    return true;
}

static List *
expr_get_vars(AstNode *expr, apr_pool_t *pool)
{
    List *var_list;

    var_list = list_make(pool);
    expr_tree_walker(expr, get_var_callback, var_list);
    return var_list;
}

/*
 * Given the currently-joined tables in "state->join_set", is it possible to
 * evaluate "qual"?
 */
static bool
join_set_satisfies_qual(AstQualifier *qual, PlannerState *state)
{
    List *var_list;
    ListCell *lc;

    var_list = expr_get_vars(qual->expr, state->tmp_pool);
    foreach (lc, var_list)
    {
        AstVarExpr *var = (AstVarExpr *) lc_ptr(lc);

        if (!join_set_satisfies_var(var, state))
            return false;
    }

    return true;
}

/*
 * Remove and return all the quals in "qual_set_todo" that can be applied to
 * the output of the join represented by "join_set".
 */
static List *
extract_matching_quals(PlannerState *state)
{
    List *result;
    ListCell *prev;
    ListCell *lc;

    result = list_make(state->tmp_pool);
    lc = list_head(state->qual_set_todo);
    prev = NULL;
    while (true)
    {
        AstQualifier *qual = (AstQualifier *) lc_ptr(lc);

        if (lc == NULL)
            break;

        if (join_set_satisfies_qual(qual, state))
        {
            list_remove_cell(state->qual_set_todo, lc, prev);
            list_append(result, qual);
            lc = prev;
        }

        prev = lc;
        lc = lc->next;
    }

    return result;
}

static void
add_scan_op(AstJoinClause *ast_join, List *quals,
            OpChainPlan *chain_plan, PlannerState *state)
{
    ScanOpPlan *splan;

    splan = apr_pcalloc(state->plan_pool, sizeof(*splan));
    splan->op_plan.op_kind = OPER_SCAN;
    splan->quals = list_copy_deep(quals, state->plan_pool);
    splan->tbl_name = apr_pstrdup(state->plan_pool, ast_join->ref->name);

    list_append(chain_plan->chain, splan);
}

static void
add_filter_op(List *quals, OpChainPlan *chain_plan, PlannerState *state)
{
    FilterOpPlan *fplan;

    fplan = apr_pcalloc(state->plan_pool, sizeof(*fplan));
    fplan->op_plan.op_kind = OPER_FILTER;
    fplan->quals = list_copy_deep(quals, state->plan_pool);
    fplan->tbl_name = chain_plan->delta_tbl;

    list_append(chain_plan->chain, fplan);
}

static void
add_delta_filter(OpChainPlan *chain_plan, PlannerState *state)
{
    List *quals;

    /* Should be the first op in the chain */
    ASSERT(list_is_empty(state->join_set));
    ASSERT(list_is_empty(chain_plan->chain));

    quals = extract_matching_quals(state);
    if (!list_is_empty(quals))
        add_filter_op(quals, chain_plan, state);
}

static void
extend_op_chain(OpChainPlan *chain_plan, PlannerState *state)
{
    AstJoinClause *candidate;
    List *quals;

    /*
     * Choose a new relation from the TODO list to add to the join set. XXX:
     * for now we just take the first element of the list.
     */
    candidate = list_remove_head(state->join_set_todo);
    list_append(state->join_set, candidate);
    list_append(state->join_set_refs, candidate->ref);

    quals = extract_matching_quals(state);
    add_scan_op(candidate, quals, chain_plan, state);
}

/*
 * Return a plan for an operator chain that begins with the given delta
 * table. We need to select the set of necessary operators and choose their
 * order. We do naive qualifier pushdown (qualifiers are associated with the
 * first node whose output contains all the variables in the qualifier).
 */
static OpChainPlan *
plan_op_chain(AstJoinClause *delta_tbl, AstRule *rule,
              RulePlan *rplan, PlannerState *state)
{
    OpChainPlan *chain_plan;
    ListCell *lc;

    state->join_set_todo = make_join_set(delta_tbl, rule->joins, state);
    state->qual_set_todo = list_copy(rule->quals, state->tmp_pool);

    state->join_set = list_make1(delta_tbl, state->tmp_pool);
    state->join_set_refs = list_make1(delta_tbl->ref, state->tmp_pool);
    state->qual_set = list_make(state->tmp_pool);

    chain_plan = apr_pcalloc(state->plan_pool, sizeof(*chain_plan));
    chain_plan->delta_tbl = apr_pstrdup(state->plan_pool,
                                        delta_tbl->ref->name);
    chain_plan->chain = list_make(state->plan_pool);

    /*
     * If there are any quals that can be applied directly to the delta
     * table, implement them via an initial Filter op.
     */
    add_delta_filter(chain_plan, state);

    while (!list_is_empty(state->join_set_todo))
        extend_op_chain(chain_plan, state);

    if (!list_is_empty(state->qual_set_todo))
        ERROR("Failed to match %d qualifiers to an operator",
              list_length(state->qual_set_todo));

    return chain_plan;
}

static RulePlan *
plan_rule(AstRule *rule, PlannerState *state)
{
    RulePlan *rplan;
    ListCell *lc;

    rplan = apr_pcalloc(state->plan_pool, sizeof(*rplan));

    /*
     * For each table referenced by the rule, generate an OpChainPlan that
     * has that table at its delta table. That is, we produce a fixed list
     * of operators that are evaluated when we see a new tuple in that
     * table.
     */
    rplan->chains = list_make(state->plan_pool);
    foreach (lc, rule->joins)
    {
        AstJoinClause *delta_tbl = (AstJoinClause *) lc_ptr(lc);
        OpChainPlan *chain_plan;

        chain_plan = plan_op_chain(delta_tbl, rule, rplan, state);
        list_append(rplan->chains, chain_plan);
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
    pplan->defines = list_copy_deep(ast->defines, pplan->pool);
    pplan->facts = list_copy_deep(ast->facts, pplan->pool);

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

void
plan_destroy(ProgramPlan *plan)
{
    apr_pool_destroy(plan->pool);
}
