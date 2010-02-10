#include "c4-internal.h"
#include "parser/walker.h"

bool
expr_tree_walker(C4Node *node, expr_callback fn, void *data)
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

        case AST_AGG_EXPR:
            {
                AstAggExpr *agg_expr = (AstAggExpr *) node;

                if (expr_tree_walker(agg_expr->expr, fn, data) == false)
                    return false;
            }
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
                    C4Node *col_expr = (C4Node *) lc_ptr(lc);

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

typedef struct VarWalkerContext
{
    var_expr_callback fn;
    void *user_data;
} VarWalkerContext;

static bool
var_walker_callback(C4Node *node, void *data)
{
    VarWalkerContext *cxt = (VarWalkerContext *) data;

    if (node->kind == AST_VAR_EXPR)
        return cxt->fn((AstVarExpr *) node, cxt->user_data);

    return true;
}

bool
expr_tree_var_walker(C4Node *node, var_expr_callback fn, void *data)
{
    VarWalkerContext cxt;

    cxt.fn = fn;
    cxt.user_data = data;

    return expr_tree_walker(node, var_walker_callback, &cxt);
}
