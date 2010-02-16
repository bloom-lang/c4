#ifndef NODES_H
#define NODES_H

typedef enum C4NodeKind
{
    /* AST nodes */
    AST_PROGRAM,
    AST_DEFINE,
    AST_TIMER,
    AST_SCHEMA_ELT,
    AST_RULE,
    AST_FACT,
    AST_TABLE_REF,
    AST_COLUMN_REF,
    AST_JOIN_CLAUSE,
    AST_QUALIFIER,
    AST_OP_EXPR,
    AST_VAR_EXPR,
    AST_CONST_EXPR,
    AST_AGG_EXPR,

    /* Plan nodes */
    PLAN_FILTER,
    PLAN_INSERT,
    PLAN_SCAN,

    /* Executor nodes */
    OPER_FILTER,
    OPER_INSERT,
    OPER_SCAN,

    /* Expr nodes */
    EXPR_OP,
    EXPR_VAR,
    EXPR_CONST
} C4NodeKind;

typedef struct C4Node
{
    C4NodeKind kind;
} C4Node;

char *node_get_kind_str(C4Node *node);

#endif  /* NODES_H */
