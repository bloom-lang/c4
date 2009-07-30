#ifndef NODES_H
#define NODES_H

typedef enum ColNodeKind
{
    AST_PROGRAM,
    AST_DEFINE,
    AST_RULE,
    AST_FACT,
    AST_TABLE_REF,
    AST_COLUMN_REF,
    AST_JOIN_CLAUSE,
    AST_QUALIFIER,
    AST_ASSIGN,
    AST_OP_EXPR,
    AST_VAR_EXPR,
    AST_CONST_EXPR
} ColNodeKind;

typedef struct ColNode
{
    ColNodeKind kind;
} ColNode;

#endif  /* NODES_H */
