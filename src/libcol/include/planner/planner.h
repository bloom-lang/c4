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
    /* Separated AST nodes */
    List *ast_joins;
    List *ast_quals;
} RulePlan;

typedef struct OpChainPlan
{
    char *delta_tbl;
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

ProgramPlan *plan_program(AstProgram *ast, ColInstance *col);
void plan_destroy(ProgramPlan *plan);

#endif  /* PLANNER_H */
