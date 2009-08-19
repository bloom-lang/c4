#ifndef EXPR_H
#define EXPR_H

#include "types/tuple.h"

typedef struct ExprState
{
    Tuple *inner;
    Tuple *outer;
} ExprState;

Datum eval_expr(ExprState *state);

#endif  /* EXPR_H */
