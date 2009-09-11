#ifndef EXPR_H
#define EXPR_H

#include "parser/ast.h"
#include "types/tuple.h"

typedef struct ExprEvalContext
{
    Tuple *inner;
    Tuple *outer;
} ExprEvalContext;

typedef struct ExprNode ExprNode;
typedef struct ExprState ExprState;

typedef Datum (*eval_expr_func)(ExprState *state);

struct ExprState
{
    ExprEvalContext *cxt;
    ExprNode *expr;
    eval_expr_func expr_func;
    ExprState *lhs;
    ExprState *rhs;
};

struct ExprNode
{
    ColNode node;
    DataType type;
};

typedef struct ExprVar
{
    ExprNode expr;
    int attno;
    bool is_outer;
    char *name;
} ExprVar;

typedef struct ExprOp
{
    ExprNode expr;
    AstOperKind op_kind;
    ExprNode *lhs;
    ExprNode *rhs;
} ExprOp;

typedef struct ExprConst
{
    ExprNode expr;
    Datum value;
} ExprConst;

#define eval_expr(state)        ((state)->expr_func(state))

ExprState *make_expr_state(ExprNode *expr, ExprEvalContext *cxt,
                           apr_pool_t *pool);
void expr_to_str(ExprNode *expr, StrBuf *sbuf);

bool eval_qual_set(int nquals, ExprState **qual_ary);

#endif  /* EXPR_H */
