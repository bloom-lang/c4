#include "c4-internal.h"
#include "nodes/nodes.h"
#include "types/expr.h"

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

static char *
get_const_kind_str(AstConstKind c_kind)
{
    switch (c_kind)
    {
        case AST_CONST_BOOL:
            return "bool";

        case AST_CONST_CHAR:
            return "char";

        case AST_CONST_DOUBLE:
            return "double";

        case AST_CONST_INT:
            return "int";

        case AST_CONST_STRING:
            return "string";

        default:
            ERROR("Unrecognized const kind: %d", (int) c_kind);
    }
}

static void
op_expr_to_str(ExprOp *op_expr, StrBuf *sbuf)
{
    sbuf_appendf(sbuf, "OP: (%s ", get_op_kind_str(op_expr->op_kind));
    node_to_str((C4Node *) op_expr->lhs, sbuf);
    if (op_expr->rhs)
        node_to_str((C4Node *) op_expr->rhs, sbuf);
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

static void
ast_op_expr_to_str(AstOpExpr *op_expr, StrBuf *sbuf)
{
    sbuf_appendf(sbuf, "AST OP: (%s ", get_op_kind_str(op_expr->op_kind));
    node_to_str(op_expr->lhs, sbuf);
    if (op_expr->rhs)
        node_to_str(op_expr->rhs, sbuf);
    sbuf_append(sbuf, ")");
}

static void
ast_const_expr_to_str(AstConstExpr *const_expr, StrBuf *sbuf)
{
    sbuf_appendf(sbuf, "AST CONST: %s (kind = %s)",
                 const_expr->value,
                 get_const_kind_str(const_expr->const_kind));
}

static void
ast_var_expr_to_str(AstVarExpr *var_expr, StrBuf *sbuf)
{
    sbuf_appendf(sbuf, "AST_VAR: name = %s, type = %s",
                 var_expr->name, get_type_name(var_expr->type));
}

static void
ast_column_ref_to_str(AstColumnRef *cref, StrBuf *sbuf)
{
    node_to_str(cref->expr, sbuf);
}

static void
ast_qual_to_str(AstQualifier *qual, StrBuf *sbuf)
{
    node_to_str(qual->expr, sbuf);
}

void
node_to_str(C4Node *node, StrBuf *sbuf)
{
    sbuf_append_char(sbuf, '(');

    switch (node->kind)
    {
        case EXPR_OP:
            op_expr_to_str((ExprOp *) node, sbuf);
            break;

        case EXPR_VAR:
            var_expr_to_str((ExprVar *) node, sbuf);
            break;

        case EXPR_CONST:
            const_expr_to_str((ExprConst *) node, sbuf);
            break;

        case AST_OP_EXPR:
            ast_op_expr_to_str((AstOpExpr *) node, sbuf);
            break;

        case AST_VAR_EXPR:
            ast_var_expr_to_str((AstVarExpr *) node, sbuf);
            break;

        case AST_CONST_EXPR:
            ast_const_expr_to_str((AstConstExpr *) node, sbuf);
            break;

        case AST_COLUMN_REF:
            ast_column_ref_to_str((AstColumnRef *) node, sbuf);
            break;

        case AST_QUALIFIER:
            ast_qual_to_str((AstQualifier *) node, sbuf);
            break;

        default:
            ERROR("Unexpected node kind: %d", (int) node->kind);
    }

    sbuf_append_char(sbuf, ')');
}

char *
node_get_kind_str(C4Node *node)
{
    switch (node->kind)
    {
        case AST_PROGRAM:
            return "AstProgram";
        case AST_DEFINE:
            return "AstDefine";
        case AST_SCHEMA_ELT:
            return "AstSchemaElt";
        case AST_RULE:
            return "AstRule";
        case AST_FACT:
            return "AstFact";
        case AST_TABLE_REF:
            return "AstTableRef";
        case AST_COLUMN_REF:
            return "AstColumnRef";
        case AST_JOIN_CLAUSE:
            return "AstJoinClause";
        case AST_QUALIFIER:
            return "AstQualifier";
        case AST_OP_EXPR:
            return "AstOpExpr";
        case AST_VAR_EXPR:
            return "AstVarExpr";
        case AST_CONST_EXPR:
            return "AstConstExpr";
        case AST_AGG_EXPR:
            return "AstAggExpr";

        case PLAN_FILTER:
            return "PlanFilter";
        case PLAN_INSERT:
            return "PlanInsert";
        case PLAN_SCAN:
            return "PlanScan";

        case OPER_AGG:
            return "OperAgg";
        case OPER_FILTER:
            return "OperFilter";
        case OPER_INSERT:
            return "OperInsert";
        case OPER_SCAN:
            return "OperScan";

        case EXPR_OP:
            return "ExprOp";
        case EXPR_VAR:
            return "ExprVar";
        case EXPR_CONST:
            return "ExprConst";

        default:
            ERROR("Unrecognized node kind: %d", (int) node->kind);
    }
}
