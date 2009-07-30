#ifndef AST_H
#define AST_H

#include "nodes/nodes.h"
#include "types/schema.h"
#include "util/list.h"

typedef struct AstTableRef AstTableRef;

typedef struct AstProgram
{
    ColNode node;
    char *name;
    List *defines;
    List *facts;
    List *rules;
} AstProgram;

typedef struct AstDefine
{
    ColNode node;
    char *name;
    List *keys;
    List *schema;
} AstDefine;

typedef struct AstFact
{
    ColNode node;
    AstTableRef *head;
} AstFact;

typedef struct AstRule
{
    ColNode node;
    char *name;
    AstTableRef *head;
    /* The rule body, divided by node kind */
    List *joins;
    List *quals;
    bool is_delete;
    /* Filled-in during parse-analysis */
    bool is_network;
} AstRule;

typedef enum AstHashVariant
{
    AST_HASH_NONE = 0,
    AST_HASH_DELETE,
    AST_HASH_INSERT
} AstHashVariant;

struct AstTableRef
{
    ColNode node;
    char *name;
    List *cols;
};

typedef struct AstColumnRef
{
    ColNode node;
    bool has_loc_spec;
    ColNode *expr;
} AstColumnRef;

typedef struct AstJoinClause
{
    ColNode node;
    AstTableRef *ref;
    bool not;
    AstHashVariant hash_variant;
} AstJoinClause;

typedef struct AstQualifier
{
    ColNode node;
    ColNode *expr;
} AstQualifier;

typedef struct AstAssign
{
    ColNode node;
    AstColumnRef *lhs;
    ColNode *rhs;
} AstAssign;

typedef enum AstOperKind
{
    AST_OP_PLUS,
    AST_OP_MINUS,
    AST_OP_TIMES,
    AST_OP_DIVIDE,
    AST_OP_MODULUS,
    AST_OP_UMINUS,
    AST_OP_LT,
    AST_OP_LTE,
    AST_OP_GT,
    AST_OP_GTE,
    AST_OP_EQ,
    AST_OP_NEQ,
    AST_OP_ASSIGN
} AstOperKind;

typedef struct AstOpExpr
{
    ColNode node;
    ColNode *lhs;
    ColNode *rhs;
    AstOperKind op_kind;
} AstOpExpr;

typedef struct AstVarExpr
{
    ColNode node;
    char *name;
    /* Filled-in by the analysis phase */
    DataType type;
} AstVarExpr;

typedef enum AstConstKind
{
    AST_CONST_BOOL,
    AST_CONST_CHAR,
    AST_CONST_DOUBLE,
    AST_CONST_INT,
    AST_CONST_STRING
} AstConstKind;

typedef struct AstConstExpr
{
    ColNode node;
    AstConstKind const_kind;
    char *value;
} AstConstExpr;

#endif  /* AST_H */
