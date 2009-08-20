#ifndef EXPR_H
#define EXPR_H

#include "parser/ast.h"
#include "types/tuple.h"

typedef struct ExprEvalContext
{
    Tuple *inner;
    Tuple *outer;
} ExprEvalContext;

typedef struct ExprState
{
    ExprEvalContext *cxt;
    ColNode *expr;
    struct ExprState *lhs;
    struct ExprState *rhs;
} ExprState;

typedef struct ExprVar
{
    ColNode node;
    int attno;
    bool is_outer;
} ExprVar;

typedef struct ExprOp
{
    ColNode node;
    AstOperKind op_kind;
} ExprOp;

Datum eval_expr(ExprState *state);

#endif  /* EXPR_H */
