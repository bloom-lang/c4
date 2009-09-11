#include "col-internal.h"
#include "types/expr.h"

static Datum
eval_op_uminus_i8(ExprState *state)
{
    Datum lhs;
    Datum result;

    ASSERT(state->lhs->expr->type == TYPE_INT8);
    ASSERT(state->expr->type == TYPE_INT8);

    lhs = eval_expr(state->lhs);
    result.i8 = -(lhs.i8);
    return result;
}

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
eval_op_minus_i8(ExprState *state)
{
    Datum lhs;
    Datum rhs;
    Datum result;

    ASSERT(state->lhs->expr->type == TYPE_INT8);
    ASSERT(state->rhs->expr->type == TYPE_INT8);
    ASSERT(state->expr->type == TYPE_INT8);

    lhs = eval_expr(state->lhs);
    rhs = eval_expr(state->rhs);
    /* XXX: check for underflow? */
    result.i8 = lhs.i8 - rhs.i8;
    return result;
}

static Datum
eval_op_times_i8(ExprState *state)
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
    result.i8 = lhs.i8 * rhs.i8;
    return result;
}

static Datum
eval_op_divide_i8(ExprState *state)
{
    Datum lhs;
    Datum rhs;
    Datum result;

    ASSERT(state->lhs->expr->type == TYPE_INT8);
    ASSERT(state->rhs->expr->type == TYPE_INT8);
    ASSERT(state->expr->type == TYPE_INT8);

    lhs = eval_expr(state->lhs);
    rhs = eval_expr(state->rhs);
    /* XXX: error checking? e.g. divide by zero */
    /* XXX: should the return type be integer or float? */
    result.i8 = lhs.i8 / rhs.i8;
    return result;
}

static Datum
eval_op_modulus_i8(ExprState *state)
{
    Datum lhs;
    Datum rhs;
    Datum result;

    ASSERT(state->lhs->expr->type == TYPE_INT8);
    ASSERT(state->rhs->expr->type == TYPE_INT8);
    ASSERT(state->expr->type == TYPE_INT8);

    lhs = eval_expr(state->lhs);
    rhs = eval_expr(state->rhs);
    /* XXX: error checking?  */
    result.i8 = lhs.i8 % rhs.i8;
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

    lhs = eval_expr(state->lhs);
    rhs = eval_expr(state->rhs);
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

    lhs = eval_expr(state->lhs);
    rhs = eval_expr(state->rhs);
    result.b = datum_equal(lhs, rhs, state->lhs->expr->type) ? true : false;
    return result;
}

static Datum
eval_op_logical(ExprState *state)
{
    ExprOp *op = (ExprOp *) state->expr;
    Datum lhs;
    Datum rhs;
    Datum result;
    int cmp_result;

    ASSERT(state->lhs->expr->type == state->rhs->expr->type);
    ASSERT(state->expr->type == TYPE_BOOL);

    lhs = eval_expr(state->lhs);
    rhs = eval_expr(state->rhs);

    cmp_result = datum_cmp(lhs, rhs, state->lhs->expr->type);
    switch (op->op_kind)
    {
        case AST_OP_LT:
            result.b = (cmp_result < 0 ? true : false);
            break;

        case AST_OP_LTE:
            result.b = (cmp_result <= 0 ? true : false);
            break;

        case AST_OP_GT:
            result.b = (cmp_result > 0 ? true : false);
            break;

        case AST_OP_GTE:
            result.b = (cmp_result >= 0 ? true : false);
            break;

        default:
            ERROR("Unexpected op kind: %d", (int) op->op_kind);
    }

    return result;
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

static eval_expr_func
lookup_op_expr_eval_func(ExprOp *op_expr)
{
    switch (op_expr->op_kind)
    {
        case AST_OP_UMINUS:
            return eval_op_uminus_i8;

        case AST_OP_PLUS:
            return eval_op_plus_i8;

        case AST_OP_MINUS:
            return eval_op_minus_i8;

        case AST_OP_TIMES:
            return eval_op_times_i8;

        case AST_OP_DIVIDE:
            return eval_op_divide_i8;

        case AST_OP_MODULUS:
            return eval_op_modulus_i8;

        case AST_OP_LT:
        case AST_OP_LTE:
        case AST_OP_GT:
        case AST_OP_GTE:
            return eval_op_logical;

        case AST_OP_EQ:
            return eval_op_eq;

        case AST_OP_NEQ:
            return eval_op_neq;

        /* AST_OP_UMINUS is handled above */
        default:
            ERROR("Unexpected op kind: %d", (int) op_expr->op_kind);
    }
}

static eval_expr_func
lookup_expr_eval_func(ExprNode *expr)
{
    switch (expr->node.kind)
    {
        case EXPR_OP:
            return lookup_op_expr_eval_func((ExprOp *) expr);

        case EXPR_VAR:
            return eval_var_expr;

        case EXPR_CONST:
            return eval_const_expr;

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
    expr_state->expr_func = lookup_expr_eval_func(expr_state->expr);

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

bool
eval_qual_set(int nquals, ExprState **qual_ary)
{
    int i;

    for (i = 0; i < nquals; i++)
    {
        ExprState *qual_state = qual_ary[i];
        Datum result;

        /*
         * Sanity check: we already require the return type of qual
         * expressions to be boolean in the analysis phase
         */
        ASSERT(qual_state->expr->type == TYPE_BOOL);

        result = eval_expr(qual_state);
        if (result.b == false)
            return false;
    }

    return true;
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
