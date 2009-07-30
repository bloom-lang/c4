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

typedef struct OpPlan
{
    OpKind op_kind;
} OpPlan;

typedef struct FilterOpPlan
{
    OpPlan op_plan;
    List *quals;
    char *tbl_name;
} FilterOpPlan;

typedef struct ScanOpPlan
{
    OpPlan op_plan;
    List *quals;
    char *tbl_name;
} ScanOpPlan;

typedef struct InsertOpPlan
{
    OpPlan op_plan;
    AstTableRef *head;
    bool is_network;
} InsertOpPlan;

ProgramPlan *plan_program(AstProgram *ast, apr_pool_t *pool, ColInstance *col);

#endif  /* PLANNER_H */
