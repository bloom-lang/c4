#include "col-internal.h"
#include "types/expr.h"

static Datum
eval_op_expr(ExprState *state)
{
    Datum d;
    return d;
}

static Datum
eval_var_expr(ExprState *state)
{
    ExprVar *var = (ExprVar *) state->expr;
    ExprEvalContext *cxt = state->cxt;

    /* XXX: bump refcount for pass-by-ref datums? */
    if (var->is_outer)
        return tuple_get_val(cxt->outer, var->attno);
    else
        return tuple_get_val(cxt->inner, var->attno);
}

static Datum
eval_const_expr(ExprState *state)
{
    Datum d;
    return d;
}

Datum
eval_expr(ExprState *state)
{
    ColNode *expr = state->expr;

    switch (expr->kind)
    {
        case EXPR_OP:
            return eval_op_expr(state);

        case EXPR_VAR:
            return eval_var_expr(state);

        case EXPR_CONST:
            return eval_const_expr(state);

        default:
            ERROR("unexpected node kind: %d", (int) expr->kind);
    }
}
