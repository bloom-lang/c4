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

/*
 * A RuleChain is a sequence of operators that begins with a "delta
 * table". When a new tuple is inserted into a table, the tuple is passed
 * down each rule chain that begins with that delta table. Any tuples that
 * emerge from the tail of the operator chain are to be inserted into the
 * "head_tbl".
 */
typedef struct RuleChain
{
    char *delta_tbl;
    char *head_tbl;
} RuleChain;

ProgramPlan *plan_program(AstProgram *ast, ColInstance *col);

#endif  /* PLANNER_H */
