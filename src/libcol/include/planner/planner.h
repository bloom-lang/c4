#ifndef PLANNER_H
#define PLANNER_H

#include "operator/operator.h"
#include "parser/ast.h"
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

typedef struct FilterPlan
{
    ColNode node;
    List *quals;
    char *tbl_name;
} FilterPlan;

typedef struct InsertPlan
{
    ColNode node;
    AstTableRef *head;
    bool is_network;
} InsertPlan;

typedef struct ScanPlan
{
    ColNode node;
    List *quals;
    char *tbl_name;
} ScanPlan;

ProgramPlan *plan_program(AstProgram *ast, apr_pool_t *pool, ColInstance *col);

#endif  /* PLANNER_H */
