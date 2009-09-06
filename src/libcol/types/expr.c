#include "col-internal.h"
#include "types/expr.h"

static Datum
eval_op_plus_i8(ExprState *state)
{
    Datum lhs;
    Datum rhs;
    Datum result;

    ASSERT(state->lhs->expr->type == TYPE_INT8);
    ASSERT(state->rhs->expr->type == TYPE_INT8);
    ASSERT(state->expr->type == TYPE_INT8);

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

    ASSERT(state->lhs->expr->type == state->rhs->expr->type);
    ASSERT(state->expr->type == TYPE_BOOL);

    lhs = eval_expr(state);
    rhs = eval_expr(state);

    result.b = datum_equal(lhs, rhs, state->lhs->expr->type);
    return result;
}

static Datum
eval_op_neq(ExprState *state)
{
    Datum lhs;
    Datum rhs;
    Datum result;

    ASSERT(state->lhs->expr->type == state->rhs->expr->type);
    ASSERT(state->expr->type == TYPE_BOOL);

    lhs = eval_expr(state);
    rhs = eval_expr(state);

    result.b = datum_equal(lhs, rhs, state->lhs->expr->type) ? true : false;
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
            ERROR("Unexpected op kind: %d", (int) op->op_kind);
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
    ExprConst *c_expr = (ExprConst *) state->expr;

    /* XXX: bump refcount for pass-by-ref datums? */
    return c_expr->value;
}

Datum
eval_expr(ExprState *state)
{
    ExprNode *expr = state->expr;

    switch (expr->node.kind)
    {
        case EXPR_OP:
            return eval_op_expr(state);

        case EXPR_VAR:
            return eval_var_expr(state);

        case EXPR_CONST:
            return eval_const_expr(state);

        default:
            ERROR("Unexpected node kind: %d", (int) expr->node.kind);
    }
}

ExprState *
make_expr_state(ExprNode *expr, ExprEvalContext *cxt, apr_pool_t *pool)
{
    ExprState *expr_state;

    expr_state = apr_pcalloc(pool, sizeof(*expr_state));
    expr_state->cxt = cxt;
    expr_state->expr = expr;

    /*
     * XXX: Slightly ugly, but we need to down-cast to determine whether we
     * need to initialize sub-ExprStates.
     */
    if (expr->node.kind == EXPR_OP)
    {
        ExprOp *op_expr = (ExprOp *) expr;

        if (op_expr->lhs)
            expr_state->lhs = make_expr_state(op_expr->lhs, cxt, pool);

        if (op_expr->rhs)
            expr_state->rhs = make_expr_state(op_expr->rhs, cxt, pool);
    }

    return expr_state;
}

static char *
get_op_kind_str(AstOperKind op_kind)
{
    switch (op_kind)
    {
        case AST_OP_PLUS:
            return "+";

        case AST_OP_MINUS:
            return "-";

        case AST_OP_TIMES:
            return "*";

        case AST_OP_DIVIDE:
            return "/";

        case AST_OP_MODULUS:
            return "%";

        case AST_OP_UMINUS:
            return "-";

        case AST_OP_LT:
            return "<";

        case AST_OP_LTE:
            return "<=";

        case AST_OP_GT:
            return ">";

        case AST_OP_GTE:
            return ">=";

        case AST_OP_EQ:
            return "==";

        case AST_OP_NEQ:
            return "!=";

        default:
            ERROR("Unrecognized op kind: %d", (int) op_kind);
    }
}

static void
op_expr_to_str(ExprOp *op_expr, StrBuf *sbuf)
{
    sbuf_append(sbuf, "OP: (");
    expr_to_str(op_expr->lhs, sbuf);
    sbuf_appendf(sbuf, ") %s (", get_op_kind_str(op_expr->op_kind));
    expr_to_str(op_expr->rhs, sbuf);
    sbuf_append(sbuf, ")");
}

static void
var_expr_to_str(ExprVar *var_expr, StrBuf *sbuf)
{
    sbuf_appendf(sbuf, "VAR: attno = %d, is_outer = %s, name = %s",
                 var_expr->attno, var_expr->is_outer ? "true" : "false",
                 var_expr->name);
}

static void
const_expr_to_str(ExprConst *const_expr, StrBuf *sbuf)
{
    sbuf_append(sbuf, "CONST: ");
    datum_to_str(const_expr->value, const_expr->expr.type, sbuf);
}

void
expr_to_str(ExprNode *expr, StrBuf *sbuf)
{
    switch (expr->node.kind)
    {
        case EXPR_OP:
            op_expr_to_str((ExprOp *) expr, sbuf);
            break;

        case EXPR_VAR:
            var_expr_to_str((ExprVar *) expr, sbuf);
            break;

        case EXPR_CONST:
            const_expr_to_str((ExprConst *) expr, sbuf);
            break;

        default:
            ERROR("Unexpected node kind: %d", (int) expr->node.kind);
    }
}
