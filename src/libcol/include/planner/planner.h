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

/*
 * An OpChain is a sequence of operators that begins with a "delta
 * table". When a new tuple is inserted into a table, the tuple is passed
 * down each op chain that begins with that delta table. Any tuples that
 * emerge from the tail of the operator chain are to be inserted into the
 * "head_tbl".
 */
typedef struct OpChain
{
    char *delta_tbl;
    char *head_tbl;
    Operator *chain_start;
    int length;
} OpChain;

ProgramPlan *plan_program(AstProgram *ast, ColInstance *col);

#endif  /* PLANNER_H */
