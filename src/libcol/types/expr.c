#include "col-internal.h"
#include "types/expr.h"

Datum
eval_expr(ExprState *state)
{
    ColNode *expr = state->expr;
    Datum d;

    switch (expr->kind)
    {
        case EXPR_OP:
        case EXPR_VAR:
        case EXPR_CONST:
            break;

        default:
            ERROR("unexpected node kind: %d", (int) expr->kind);
    }

    return d;
}
