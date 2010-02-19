/*
 * Basic planner algorithm is described below. Doesn't handle negation,
 * aggregation, or stratification. No significant optimization is performed
 * (e.g. no intelligent selection of join order). We always perform
 * predicate pushdown.
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
#include "c4-internal.h"
#include "nodes/copyfuncs.h"
#include "nodes/makefuncs.h"
#include "parser/parser.h"
#include "parser/walker.h"
#include "planner/planner.h"
#include "planner/planner-internal.h"

static ProgramPlan *
program_plan_make(apr_pool_t *pool)
{
    ProgramPlan *pplan;

    pplan = apr_pcalloc(pool, sizeof(*pplan));
    pplan->pool = pool;
    /* Remaining fields filled-in by caller */

    return pplan;
}

static PlannerState *
planner_state_make(apr_pool_t *plan_pool, C4Runtime *c4)
{
    PlannerState *state;
    apr_pool_t *tmp_pool;

    tmp_pool = make_subpool(plan_pool);
    state = apr_pcalloc(tmp_pool, sizeof(*state));
    state->c4 = c4;
    state->plan_pool = plan_pool;
    state->tmp_pool = tmp_pool;
    state->plan = program_plan_make(plan_pool);
    state->current_plist = NULL;

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

            ASSERT(cref->expr->kind == AST_VAR_EXPR);
            column_var = (AstVarExpr *) cref->expr;
            if (strcmp(var->name, column_var->name) == 0)
                return true;
        }
    }

    return false;
}

static bool
get_var_callback(AstVarExpr *var, void *data)
{
    List *var_list = (List *) data;

    list_append(var_list, var);
    return true;
}

static List *
expr_get_vars(C4Node *expr, apr_pool_t *pool)
{
    List *var_list;

    var_list = list_make(pool);
    expr_tree_var_walker(expr, get_var_callback, var_list);
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
    while (lc != NULL)
    {
        AstQualifier *qual = (AstQualifier *) lc_ptr(lc);

        if (join_set_satisfies_qual(qual, state))
        {
            list_remove_cell(state->qual_set_todo, lc, prev);
            list_append(result, qual);
        }
        else
        {
            prev = lc;
        }

        lc = lc->next;
    }

    return result;
}

static void
add_scan_op(AstJoinClause *ast_join, List *quals,
            OpChainPlan *chain_plan, PlannerState *state)
{
    ScanPlan *splan;

    splan = make_scan_plan(ast_join, quals, NULL, NULL, state->plan_pool);
    list_append(chain_plan->chain, splan);
}

static void
add_filter_op(List *quals, OpChainPlan *chain_plan, PlannerState *state)
{
    FilterPlan *fplan;

    fplan = make_filter_plan(chain_plan->delta_tbl->ref->name, quals,
                             NULL, NULL, state->plan_pool);
    list_append(chain_plan->chain, fplan);
}

static void
add_insert_op(AstRule *rule, OpChainPlan *chain_plan, PlannerState *state)
{
    InsertPlan *iplan;

    iplan = make_insert_plan(rule->head, chain_plan->delta_tbl->not, NULL,
                             state->plan_pool);
    list_append(chain_plan->chain, iplan);
}

static void
add_delta_filter(OpChainPlan *chain_plan, PlannerState *state)
{
    List *quals;

    /* Only the delta table should be in the join set */
    ASSERT(list_length(state->join_set) == 1);
    /* Should be the first op in the chain */
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
 *
 * The last op in the chain (and the first one created) is an OPER_INSERT,
 * which inserts tuples into the head table (and/or enqueues them to be sent
 * over the network).
 */
static OpChainPlan *
plan_op_chain(AstJoinClause *delta_tbl, AstRule *rule, PlannerState *state)
{
    OpChainPlan *chain_plan;

    state->join_set_todo = make_join_set(delta_tbl, rule->joins, state);
    state->qual_set_todo = list_copy(rule->quals, state->tmp_pool);

    state->join_set = list_make1(delta_tbl, state->tmp_pool);
    state->join_set_refs = list_make1(delta_tbl->ref, state->tmp_pool);
    state->qual_set = list_make(state->tmp_pool);

    chain_plan = apr_pcalloc(state->plan_pool, sizeof(*chain_plan));
    chain_plan->delta_tbl = copy_node(delta_tbl, state->plan_pool);
    chain_plan->head = copy_node(rule->head, state->plan_pool);
    chain_plan->chain = list_make(state->plan_pool);

    /*
     * If there are any quals that can be applied directly to the delta
     * table, implement them via an initial Filter operator.
     */
    add_delta_filter(chain_plan, state);

    while (!list_is_empty(state->join_set_todo))
        extend_op_chain(chain_plan, state);

    if (!list_is_empty(state->qual_set_todo))
        ERROR("Failed to match %d qualifiers to an operator",
              list_length(state->qual_set_todo));

    add_insert_op(rule, chain_plan, state);

    /*
     * Compute the projection list required for each operator, and fixup
     * expressions so that they are relative to the post-projection
     * representation of the operator's input.
     */
    fix_chain_exprs(chain_plan, state);

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
     * has that table as its delta table. That is, we produce a fixed list
     * of operators that are evaluated when we see a new tuple in that
     * table.
     */
    rplan->chains = list_make(state->plan_pool);
    foreach (lc, rule->joins)
    {
        AstJoinClause *delta_tbl = (AstJoinClause *) lc_ptr(lc);
        OpChainPlan *chain_plan;

        chain_plan = plan_op_chain(delta_tbl, rule, state);
        list_append(rplan->chains, chain_plan);

        /* We currently use the first join for the bootstrap table */
        if (rplan->bootstrap_tbl == NULL)
            rplan->bootstrap_tbl = delta_tbl->ref;
    }

    ASSERT(rplan->bootstrap_tbl != NULL);
    return rplan;
}

ProgramPlan *
plan_program(AstProgram *ast, apr_pool_t *pool, C4Runtime *c4)
{
    PlannerState *state;
    ProgramPlan *pplan;
    ListCell *lc;

    state = planner_state_make(pool, c4);
    pplan = state->plan;

    pplan->defines = list_copy_deep(ast->defines, pplan->pool);
    pplan->timers = list_copy_deep(ast->timers, pplan->pool);
    pplan->facts = list_copy_deep(ast->facts, pplan->pool);
    pplan->rules = list_make(pplan->pool);

    foreach (lc, ast->rules)
    {
        AstRule *rule = (AstRule *) lc_ptr(lc);
        RulePlan *rplan;

        rplan = plan_rule(rule, state);
        list_append(pplan->rules, rplan);
    }

    /* Cleanup planner working state */
    apr_pool_destroy(state->tmp_pool);
    return pplan;
}

void
print_plan_info(PlanNode *plan, apr_pool_t *p)
{
    StrBuf *sbuf;
    ListCell *lc;

    sbuf = sbuf_make(p);
    sbuf_appendf(sbuf, "PLAN: %s\n", node_get_kind_str(&plan->node));

    sbuf_append(sbuf, "  QUALS (Ast): [");
    foreach (lc, plan->quals)
    {
        C4Node *node = (C4Node *) lc_ptr(lc);

        sbuf_append(sbuf, node_get_kind_str(node));
        if (lc != list_tail(plan->quals))
            sbuf_append(sbuf, ", ");
    }
    sbuf_append(sbuf, "]\n");

    sbuf_append(sbuf, "  QUALS (Exec): [");
    foreach (lc, plan->qual_exprs)
    {
        ExprNode *expr = (ExprNode *) lc_ptr(lc);

        expr_to_str(expr, sbuf);
        if (lc != list_tail(plan->qual_exprs))
            sbuf_append(sbuf, ", ");
    }
    sbuf_append(sbuf, "]\n");

    sbuf_append(sbuf, "  PROJ LIST: [");
    foreach (lc, plan->proj_list)
    {
        ExprNode *expr = (ExprNode *) lc_ptr(lc);

        expr_to_str(expr, sbuf);
        if (lc != list_tail(plan->proj_list))
            sbuf_append(sbuf, ", ");
    }
    sbuf_append(sbuf, "]\n");

    /* Print extra info about certain plan types */
    switch (plan->node.kind)
    {
        case PLAN_INSERT:
            {
                InsertPlan *iplan = (InsertPlan *) plan;
                sbuf_appendf(sbuf, "  HEAD: %s\n", iplan->head->name);
            }
            break;

        case PLAN_SCAN:
            {
                ScanPlan *splan = (ScanPlan *) plan;
                sbuf_appendf(sbuf, "  SCAN REL: %s\n ; ANTISCAN: %s",
                             splan->scan_rel->ref->name,
                             splan->scan_rel->not ? "true" : "false");
            }
            break;

        default:
            break; /* Do nothing */
    }

    sbuf_append_char(sbuf, '\0');
    printf("%s", sbuf->data);
}
