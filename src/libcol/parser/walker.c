#include "col-internal.h"
#include "parser/walker.h"

bool
expr_tree_walker(ColNode *node, expr_callback fn, void *data)
{
    if (!node)
        return true;

    switch (node->kind)
    {
        case AST_OP_EXPR:
            {
                AstOpExpr *op_expr = (AstOpExpr *) node;

                if (expr_tree_walker(op_expr->lhs, fn, data) == false)
                    return false;

                if (expr_tree_walker(op_expr->rhs, fn, data) == false)
                    return false;
            }
            break;

        case AST_VAR_EXPR:
        case AST_CONST_EXPR:
            break;

        case AST_QUALIFIER:
            {
                AstQualifier *ast_qual = (AstQualifier *) node;

                if (expr_tree_walker(ast_qual->expr, fn, data) == false)
                    return false;
            }
            break;

        case AST_TABLE_REF:
            {
                AstTableRef *tbl_ref = (AstTableRef *) node;
                ListCell *lc;

                foreach (lc, tbl_ref->cols)
                {
                    ColNode *col_expr = (ColNode *) lc_ptr(lc);

                    if (expr_tree_walker(col_expr, fn, data) == false)
                        return false;
                }
            }
            break;

        case AST_COLUMN_REF:
            {
                AstColumnRef *cref = (AstColumnRef *) node;

                if (expr_tree_walker(cref->expr, fn, data) == false)
                    return false;
            }
            break;

        default:
            ERROR("Unrecognized expression node kind: %d",
                  (int) node->kind);
    }

    return fn(node, data);
}
