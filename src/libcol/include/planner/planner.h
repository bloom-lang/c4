#ifndef PLANNER_H
#define PLANNER_H

#include "operator/operator.h"
#include "parser/ast.h"
#include "parser/parser.h"
#include "util/list.h"

typedef struct ProgramPlan
{
    apr_pool_t *pool;
    char *name;
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
    char *delta_tbl;
    AstTableRef *head;
    List *chain;
} OpChainPlan;

typedef struct PlanNode
{
    ColNode node;
    List *quals;
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
    char *tbl_name;
} ScanPlan;

ProgramPlan *plan_program(ParseResult *pr, apr_pool_t *pool, ColInstance *col);

#endif  /* PLANNER_H */
