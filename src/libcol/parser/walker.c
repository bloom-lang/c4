#include "col-internal.h"
#include "parser/walker.h"

void
expr_tree_walker(AstNode *node, expr_callback fn, void *data)
{
    if (!node)
        return;

    switch (node->kind)
    {
        case AST_OP_EXPR:
        {
            AstOpExpr *op_expr = (AstOpExpr *) node;

            expr_tree_walker(op_expr->lhs, fn, data);
            expr_tree_walker(op_expr->rhs, fn, data);
            break;
        }

        case AST_VAR_EXPR:
        case AST_CONST_EXPR:
            break;

        default:
            ERROR("Unrecognized expression node kind: %d",
                  (int) node->kind);
    }

    fn(node, data);
}
