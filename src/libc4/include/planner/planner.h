#ifndef PLANNER_H
#define PLANNER_H

#include "parser/ast.h"
#include "util/list.h"

typedef struct ProgramPlan
{
    apr_pool_t *pool;
    List *defines;
    List *facts;
    List *rules;
} ProgramPlan;

typedef struct RulePlan
{
    List *chains;
} RulePlan;

typedef struct OpChainPlan
{
    AstJoinClause *delta_tbl;
    AstTableRef *head;
    /* A PlanNode for each op in the chain */
    List *chain;
} OpChainPlan;

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

typedef struct FilterPlan
{
    PlanNode plan;
    char *tbl_name;
} FilterPlan;

typedef struct InsertPlan
{
    PlanNode plan;
    AstTableRef *head;
    bool is_network;
} InsertPlan;

typedef struct ScanPlan
{
    PlanNode plan;
    AstJoinClause *scan_rel;
} ScanPlan;

ProgramPlan *plan_program(AstProgram *ast, apr_pool_t *pool, C4Instance *c4);
void print_plan_info(PlanNode *plan, apr_pool_t *p);

#endif  /* PLANNER_H */
