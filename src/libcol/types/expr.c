#include "col-internal.h"
#include "types/expr.h"

static Datum
eval_op_plus_i8(ExprState *state)
{
    Datum lhs;
    Datum rhs;
    Datum result;

    ASSERT(state->lhs->type == TYPE_INT8);
    ASSERT(state->rhs->type == TYPE_INT8);
    ASSERT(state->type == TYPE_INT8);

    lhs = eval_expr(state->lhs);
    rhs = eval_expr(state->rhs);

    /* XXX: check for overflow? */
    result.i8 = lhs.i8 + rhs.i8;
    return result;
}

static Datum
eval_op_eq(ExprState *state)
{
    Datum lhs;
    Datum rhs;
    Datum result;

    ASSERT(state->lhs->type == state->rhs->type);
    ASSERT(state->type == TYPE_BOOL);

    lhs = eval_expr(state);
    rhs = eval_expr(state);

    result.b = datum_equal(lhs, rhs, state->lhs->type);
    return result;
}

static Datum
eval_op_neq(ExprState *state)
{
    Datum lhs;
    Datum rhs;
    Datum result;

    ASSERT(state->lhs->type == state->rhs->type);
    ASSERT(state->type == TYPE_BOOL);

    lhs = eval_expr(state);
    rhs = eval_expr(state);

    result.b = datum_equal(lhs, rhs, state->lhs->type) ? true : false;
    return result;
}

static Datum
eval_op_expr(ExprState *state)
{
    ExprOp *op = (ExprOp *) state->expr;

    switch (op->op_kind)
    {
        case AST_OP_PLUS:
            return eval_op_plus_i8(state);

        case AST_OP_EQ:
            return eval_op_eq(state);

        case AST_OP_NEQ:
            return eval_op_neq(state);

        default:
            ERROR("unexpected op kind: %d", (int) op->op_kind);
    }
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
