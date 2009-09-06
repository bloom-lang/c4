#ifndef OPERATOR_H
#define OPERATOR_H

#include "parser/ast.h"
#include "planner/planner.h"
#include "types/tuple.h"

typedef struct Operator Operator;
typedef struct OpChain OpChain;

/*
 * An OpChain is a sequence of operators that begins with a "delta
 * table". When a new tuple is inserted into a table, the tuple is passed
 * down each op chain that begins with that delta table. The last operator
 * in the chain is an OPER_INSERT that inserts tuples into the "head" table,
 * and/or enqueues them to be sent over the network.
 */
struct OpChain
{
    apr_pool_t *pool;
    char *delta_tbl;
    AstTableRef *head;
    Operator *chain_start;
    int length;

    /*
     * In the router, a pointer to the next op chain for the same delta
     * table
     */
    OpChain *next;
};

typedef void (*op_invoke_func)(Operator *op, Tuple *t);
typedef void (*op_destroy_func)(Operator *op);

struct Operator
{
    ColNode node;
    apr_pool_t *pool;
    Operator *next;
    PlanNode *plan;

    op_invoke_func invoke;
    op_destroy_func destroy;
};

/* Generic support routines for operators */
Operator *operator_make(ColNodeKind kind, apr_size_t sz, PlanNode *plan,
                        Operator *next_op, op_invoke_func invoke_f,
                        op_destroy_func destroy_f, apr_pool_t *pool);
void operator_destroy(Operator *op);

#endif  /* OPERATOR_H */
