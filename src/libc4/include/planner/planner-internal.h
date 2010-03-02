#ifndef PLANNER_INTERNAL_H
#define PLANNER_INTERNAL_H

#include <apr_hash.h>

#include "planner/planner.h"
#include "types/expr.h"

typedef struct PlannerState
{
    C4Runtime *c4;
    ProgramPlan *plan;
    List *join_set_todo;
    List *qual_set_todo;
    List *join_set;
    List *qual_set;
    List *join_set_refs;

    /* The set of variable names projected by the current operator */
    List *current_plist;
    /* Map from var name => list of equal variables name, s.t. the equality has
     * been applied by a previous element in the current OpChain */
    apr_hash_t *var_eq_tbl;

    /* The pool in which the output plan is allocated */
    apr_pool_t *plan_pool;
    /*
     * Storage for temporary allocations made during the planning
     * process. This is a subpool of the plan pool; it is deleted when
     * planning is complete.
     */
    apr_pool_t *tmp_pool;
} PlannerState;

/* plan_expr.c */
void fix_chain_exprs(OpChainPlan *chain_plan, PlannerState *state);

#endif  /* PLANNER_INTERNAL_H */
