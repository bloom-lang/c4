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
#include "col-internal.h"
#include "nodes/copyfuncs.h"
#include "nodes/makefuncs.h"
#include "parser/parser.h"
#include "parser/walker.h"
#include "planner/planner.h"
#include "types/expr.h"

typedef struct PlannerState
{
    ColInstance *col;
    ProgramPlan *plan;
    List *join_set_todo;
    List *qual_set_todo;
    List *join_set;
    List *qual_set;
    List *join_set_refs;

    /* The set of variable names projected by the current operator */
    List *current_plist;

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
    /* Remaining fields filled-in by caller */

    return pplan;
}

static PlannerState *
planner_state_make(AstProgram *ast, apr_pool_t *plan_pool, ColInstance *col)
{
    PlannerState *state;
    apr_pool_t *tmp_pool;

    tmp_pool = make_subpool(plan_pool);
    state = apr_pcalloc(tmp_pool, sizeof(*state));
    state->col = col;
    state->plan_pool = plan_pool;
    state->tmp_pool = tmp_pool;
    state->plan = program_plan_make(ast, plan_pool);
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
get_var_callback(ColNode *n, void *data)
{
    List *var_list = (List *) data;

    if (n->kind == AST_VAR_EXPR)
        list_append(var_list, n);

    return true;
}

static List *
expr_get_vars(ColNode *expr, apr_pool_t *pool)
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

    splan = make_scan_plan(ast_join, quals, state->plan_pool);
    list_append(chain_plan->chain, splan);
}

static void
add_filter_op(List *quals, OpChainPlan *chain_plan, PlannerState *state)
{
    FilterPlan *fplan;

    fplan = make_filter_plan(chain_plan->delta_tbl->ref->name, quals,
                             state->plan_pool);
    list_append(chain_plan->chain, fplan);
}

static void
add_insert_op(AstRule *rule, OpChainPlan *chain_plan, PlannerState *state)
{
    InsertPlan *iplan;

    iplan = make_insert_plan(rule->head, rule->is_network, state->plan_pool);
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

static Datum
eval_const_expr(AstConstExpr *ast_const, PlannerState *state)
{
    Datum d;

    return d;
}

static int
get_var_index_from_join(const char *var_name, AstJoinClause *join)
{
    ListCell *lc;
    int i;

    i = 0;
    foreach (lc, join->ref->cols)
    {
        AstColumnRef *col = (AstColumnRef *) lc_ptr(lc);
        AstVarExpr *var;

        ASSERT(col->expr->kind == AST_VAR_EXPR);
        var = (AstVarExpr *) col->expr;

        if (strcmp(var->name, var_name) == 0)
            return i;

        i++;
    }

    return -1;
}

static int
get_var_index_from_plist(const char *var_name, List *proj_list)
{
    ListCell *lc;
    int i;

    i = 0;
    foreach (lc, proj_list)
    {
        char *s = (char *) lc_ptr(lc);

        if (strcmp(var_name, s) == 0)
            return i;

        i++;
    }

    return -1;
}

static ColNode *
fix_qual_expr(ColNode *ast_qual, AstJoinClause *outer_rel,
              OpChainPlan *chain_plan, PlannerState *state)
{
    ColNode *result;

    switch (ast_qual->kind)
    {
        case AST_CONST_EXPR:
            {
                AstConstExpr *ast_const = (AstConstExpr *) ast_qual;
                Datum const_val;
                ExprConst *expr_const;

                const_val = eval_const_expr(ast_const, state);
                expr_const = apr_pcalloc(state->plan_pool, sizeof(*expr_const));
                expr_const->node.kind = EXPR_CONST;
                expr_const->value = const_val;
                result = (ColNode *) expr_const;
            }
            break;

        case AST_OP_EXPR:
            {
                AstOpExpr *ast_op = (AstOpExpr *) ast_qual;
                ExprOp *expr_op;

                expr_op = apr_pcalloc(state->plan_pool, sizeof(*expr_op));
                expr_op->node.kind = EXPR_OP;
                expr_op->op_kind = ast_op->op_kind;
                expr_op->lhs = fix_qual_expr(ast_op->lhs, outer_rel, chain_plan, state);
                expr_op->rhs = fix_qual_expr(ast_op->rhs, outer_rel, chain_plan, state);
                result = (ColNode *) expr_op;
            }
            break;

        case AST_VAR_EXPR:
            {
                AstVarExpr *ast_var = (AstVarExpr *) ast_qual;
                int var_idx;
                bool is_outer = false;
                ExprVar *expr_var;

                /*
                 * If we haven't done projection yet, we need to look for
                 * variables in the delta table's join clause. therwise use
                 * the current projection list.
                 */
                if (state->current_plist == NULL)
                    var_idx = get_var_index_from_join(ast_var->name, chain_plan->delta_tbl);
                else
                    var_idx = get_var_index_from_plist(ast_var->name, state->current_plist);

                /*
                 * If the variable isn't in the projection list of the input
                 * to this operator, this must be a join and the variable is
                 * the probed relation.
                 */
                if (var_idx == -1 && outer_rel != NULL)
                {
                    is_outer = true;
                    var_idx = get_var_index_from_join(ast_var->name, outer_rel);
                }

                if (var_idx == -1)
                    ERROR("Failed to find variable %s", ast_var->name);

                expr_var = apr_pcalloc(state->plan_pool, sizeof(*expr_var));
                expr_var->node.kind = EXPR_VAR;
                expr_var->attno = var_idx;
                expr_var->is_outer = is_outer;
                result = (ColNode *) expr_var;
            }
            break;

        default:
            ERROR("Unexpected expr node kind: %d", (int) ast_qual->kind);
    }

    return result;
}

static void
fix_op_exprs(PlanNode *plan, ListCell *chain_rest,
             OpChainPlan *chain_plan, PlannerState *state)
{
    AstJoinClause *scan_rel = NULL;
    ListCell *lc;

    if (plan->node.kind == PLAN_SCAN)
    {
        ScanPlan *scan_plan = (ScanPlan *) plan;
        scan_rel = scan_plan->scan_rel;
    }

    ASSERT(plan->qual_exprs == NULL);
    plan->qual_exprs = list_make(state->plan_pool);

    foreach (lc, plan->quals)
    {
        AstQualifier *qual = (AstQualifier *) lc_ptr(lc);
        ColNode *expr;

        expr = fix_qual_expr(qual->expr, scan_rel, chain_plan, state);
        list_append(plan->qual_exprs, expr);
    }

    /* XXX: Use chain_rest to compute the projection list for this operator */
    /* XXX: Update current_plist */
}

/*
 * Walk the operator chain from beginning to end. For each operator, (a)
 * compute the minimal projection list that contains all the variables
 * required by subsequent operators in the chain (b) convert Expr from AST
 * representation to Eval representation.
 */
static void
fix_chain_exprs(OpChainPlan *chain_plan, PlannerState *state)
{
    ListCell *lc;

    /* Initial proj list is empty => no projection before delta filter */
    state->current_plist = NULL;

    foreach (lc, chain_plan->chain)
    {
        PlanNode *node = (PlanNode *) lc_ptr(lc);

        fix_op_exprs(node, lc->next, chain_plan, state);
    }
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
        printf("%p: delta_tbl = %s, chain len = %d\n", (void *) chain_plan,
               chain_plan->delta_tbl->ref->name, list_length(chain_plan->chain));
    }

    return rplan;
}

ProgramPlan *
plan_program(AstProgram *ast, apr_pool_t *pool, ColInstance *col)
{
    PlannerState *state;
    ProgramPlan *pplan;
    ListCell *lc;

    state = planner_state_make(ast, pool, col);
    pplan = state->plan;

    pplan->defines = list_copy_deep(ast->defines, pplan->pool);
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
