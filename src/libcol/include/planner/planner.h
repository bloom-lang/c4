#ifndef PLANNER_H
#define PLANNER_H

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

ProgramPlan *plan_program(AstProgram *ast, ColInstance *col);
void plan_destroy(ProgramPlan *plan);

#endif  /* PLANNER_H */
