#ifndef OPERATOR_H
#define OPERATOR_H

#include "parser/ast.h"
#include "planner/planner.h"
#include "types/catalog.h"
#include "types/expr.h"
#include "types/tuple.h"

typedef struct Operator Operator;
typedef struct OpChain OpChain;

/*
 * An OpChain is a sequence of operators that begins with a "delta
 * table". When a new tuple is inserted into a table, the tuple is passed
 * down each op chain that begins with that delta table.
 */
struct OpChain
{
    apr_pool_t *pool;
    C4Runtime *c4;
    TableDef *delta_tbl;
    AstTableRef *head;
    bool anti_chain;
    Operator *chain_start;
    int length;

    /*
     * In the router, a pointer to the next op chain for the same delta
     * table
     */
    OpChain *next;
};

typedef void (*op_invoke_func)(Operator *op, Tuple *t);

struct Operator
{
    C4Node node;
    apr_pool_t *pool;
    PlanNode *plan;
    Operator *next;
    ExprEvalContext *exec_cxt;
    OpChain *chain;

    /* Projection info */
    int nproj;
    ExprState **proj_ary;
    Schema *proj_schema;

    op_invoke_func invoke;
};

/*
 * A list of OpChains that all have the same delta_tbl. This is just used
 * for convenience in the router (specifically, we need something for
 * TableDef to point at, even if the list happens to be empty).
 */
typedef struct OpChainList
{
    OpChain *head;
    int length;
} OpChainList;

/* Generic support routines for operators */
Operator *operator_make(C4NodeKind kind, apr_size_t sz, PlanNode *plan,
                        Operator *next_op, OpChain *chain,
                        op_invoke_func invoke_f);

Tuple *operator_do_project(Operator *op);

OpChainList *opchain_list_make(apr_pool_t *pool);
void opchain_list_add(OpChainList *list, OpChain *op_chain);

#endif  /* OPERATOR_H */
