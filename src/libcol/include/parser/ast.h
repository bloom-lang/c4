#ifndef AST_H
#define AST_H

#include "util/list.h"

typedef enum AstNodeKind
{
    AST_PROGRAM,
    AST_DEFINE,
    AST_RULE,
    AST_FACT,
    AST_TABLE_REF,
    AST_COLUMN_REF,
    AST_ASSIGN
} AstNodeKind;

typedef struct AstNode
{
    AstNodeKind kind;
} AstNode;

typedef struct AstProgram
{
    AstNode node;
    char *name;
    List *clauses;
} AstProgram;

typedef struct AstDefine
{
    AstNode node;
    char *name;
    List *keys;
    List *schema;
} AstDefine;

typedef enum AstHashVariant
{
    AST_HASH_NONE = 0,
    AST_HASH_DELETE,
    AST_HASH_INSERT
} AstHashVariant;

typedef struct AstTableRef
{
    AstNode node;
    char *name;
    AstHashVariant hash_variant;
    List *cols;
} AstTableRef;

typedef struct AstColumnRef
{
    AstNode node;
    bool has_loc_spec;
    char *name;
} AstColumnRef;

typedef struct AstRule
{
    AstNode node;
    bool is_delete;
    AstTableRef *head;
    List *body;
} AstRule;

typedef struct AstFact
{
    AstNode node;
    AstTableRef *head;
} AstFact;

typedef struct AstExpr
{
    AstNode node;
} AstExpr;

typedef struct AstAssign
{
    AstNode node;
    AstColumnRef *lhs;
    AstExpr *rhs;
} AstAssign;

#endif  /* AST_H */
