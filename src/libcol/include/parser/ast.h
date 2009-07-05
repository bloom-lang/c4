#ifndef AST_H
#define AST_H

#include "types/schema.h"
#include "util/list.h"

typedef struct AstTableRef AstTableRef;

typedef enum AstNodeKind
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
} AstNodeKind;

typedef struct AstNode
{
    AstNodeKind kind;
} AstNode;

typedef struct AstProgram
{
    AstNode node;
    char *name;
    List *defines;
    List *facts;
    List *rules;
} AstProgram;

typedef struct AstDefine
{
    AstNode node;
    char *name;
    List *keys;
    List *schema;
} AstDefine;

typedef struct AstFact
{
    AstNode node;
    AstTableRef *head;
} AstFact;

typedef struct AstRule
{
    AstNode node;
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
    AstNode node;
    char *name;
    List *cols;
};

typedef struct AstColumnRef
{
    AstNode node;
    bool has_loc_spec;
    AstNode *expr;
} AstColumnRef;

typedef struct AstJoinClause
{
    AstNode node;
    AstTableRef *ref;
    bool not;
    AstHashVariant hash_variant;
} AstJoinClause;

typedef struct AstQualifier
{
    AstNode node;
    AstNode *expr;
} AstQualifier;

typedef struct AstAssign
{
    AstNode node;
    AstColumnRef *lhs;
    AstNode *rhs;
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
    AstNode node;
    AstNode *lhs;
    AstNode *rhs;
    AstOperKind op_kind;
} AstOpExpr;

typedef struct AstVarExpr
{
    AstNode node;
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

typedef union AstConstValue
{
    bool b;
    unsigned char c;
    apr_int64_t i;
    /* Used to hold both floating point and string constants */
    char *s;
} AstConstValue;

typedef struct AstConstExpr
{
    AstNode node;
    AstConstKind const_kind;
    AstConstValue value;
} AstConstExpr;

#endif  /* AST_H */
