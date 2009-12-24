#include "col-internal.h"
#include "nodes/nodes.h"

char *
node_get_kind_str(ColNode *node)
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

        case PLAN_FILTER:
            return "PlanFilter";
        case PLAN_INSERT:
            return "PlanInsert";
        case PLAN_SCAN:
            return "PlanScan";

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
