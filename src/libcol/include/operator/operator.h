#ifndef OPERATOR_H
#define OPERATOR_H

#include "parser/ast.h"
#include "types/tuple.h"

typedef struct Operator Operator;

/*
 * An OpChain is a sequence of operators that begins with a "delta
 * table". When a new tuple is inserted into a table, the tuple is passed
 * down each op chain that begins with that delta table. Any tuples that
 * emerge from the tail of the operator chain are to be inserted into the
 * "head_tbl".
 */
typedef struct OpChain
{
    apr_pool_t *pool;
    char *delta_tbl;
    AstTableRef *head;
    Operator *chain_start;
    int length;
} OpChain;

typedef enum OpKind
{
    OPER_SCAN,
    OPER_FILTER
} OpKind;

typedef void (*op_invoke_func)(Operator *op, Tuple *t);
typedef void (*op_destroy_func)(Operator *op);

struct Operator
{
    OpKind op_kind;
    apr_pool_t *pool;
    Operator *next;

    op_invoke_func invoke;
    op_destroy_func destroy;
};

/* Generic support routines for operators */
Operator *operator_make(OpKind op_kind, apr_size_t sz, Operator *next_op,
                        op_invoke_func invoke_f, op_destroy_func destroy_f,
                        apr_pool_t *pool);
void operator_destroy(Operator *op);

#endif  /* OPERATOR_H */
