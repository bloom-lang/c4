#ifndef PLANNER_H
#define PLANNER_H

#include "parser/ast.h"
#include "util/list.h"

typedef struct PlanNode
{
    C4Node node;
    /* AST representation of quals: list of AstQualifier */
    List *quals;
    /* Runtime representation of quals */
    List *qual_exprs;
    /* Runtime representation of proj list */
    List *proj_list;
} PlanNode;

typedef struct AggPlan
{
    PlanNode plan;
    AstTableRef *head;
    bool planned;
} AggPlan;

typedef struct FilterPlan
{
    PlanNode plan;
    char *tbl_name;
} FilterPlan;

typedef struct InsertPlan
{
    PlanNode plan;
    AstTableRef *head;
} InsertPlan;

typedef struct ProjectPlan
{
    PlanNode plan;
} ProjectPlan;

typedef struct ScanPlan
{
    PlanNode plan;
    AstJoinClause *scan_rel;
} ScanPlan;

typedef struct ProgramPlan
{
    apr_pool_t *pool;
    List *defines;
    List *timers;
    List *facts;
    List *rules;
} ProgramPlan;

typedef struct OpChainPlan
{
    AstJoinClause *delta_tbl;
    AstTableRef *head;
    /* A PlanNode for each op in the chain */
    List *chain;
} OpChainPlan;

typedef struct RulePlan
{
    List *chains;
    /*
     * While most plan nodes are re-created for each op chain, we share a single
     * AggPlan among all the chains for a given rule. This is because we want to
     * do grouping/aggregation over the entire rule output.
     */
    AggPlan *agg_plan;
    /*
     * When a rule is installed, we need to trigger deltas for facts that
     * existed prior to the rule's definition. We do this by picking an
     * arbitrary "bootstrap" table from the rule's body terms, and re-inserting
     * the entire content of that table after installing the plan.
     */
    AstTableRef *bootstrap_tbl;
} RulePlan;

ProgramPlan *plan_program(AstProgram *ast, apr_pool_t *pool, C4Runtime *c4);
void print_plan_info(PlanNode *plan, apr_pool_t *p);

#endif  /* PLANNER_H */
