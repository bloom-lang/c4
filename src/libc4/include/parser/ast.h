#ifndef AST_H
#define AST_H

#include "nodes/nodes.h"
#include "types/schema.h"
#include "util/list.h"

typedef struct AstTableRef AstTableRef;

typedef struct AstProgram
{
    C4Node node;
    List *defines;
    List *timers;
    List *facts;
    List *rules;
} AstProgram;

typedef enum AstStorageKind
{
    AST_STORAGE_AGG,
    AST_STORAGE_MEMORY,
    AST_STORAGE_SQLITE
} AstStorageKind;

typedef struct AstDefine
{
    C4Node node;
    char *name;
    AstStorageKind storage;
    List *keys;
    List *schema;
} AstDefine;

typedef struct AstTimer
{
    C4Node node;
    char *name;
    apr_int64_t period;         /* Timer period in milliseconds */
} AstTimer;

typedef struct AstSchemaElt
{
    C4Node node;
    char *type_name;
    bool is_loc_spec;
} AstSchemaElt;

typedef struct AstFact
{
    C4Node node;
    AstTableRef *head;
} AstFact;

typedef struct AstRule
{
    C4Node node;
    char *name;
    AstTableRef *head;
    /* The rule body, divided by node kind */
    List *joins;
    List *quals;
    bool is_delete;
    /* Filled-in during parse-analysis */
    bool is_network;
} AstRule;

struct AstTableRef
{
    C4Node node;
    /* The name of the referenced relation */
    char *name;
    /* The list of AstColumnRef that are bound to the table's columns */
    List *cols;
};

/* XXX: get rid of this? */
typedef struct AstColumnRef
{
    C4Node node;
    C4Node *expr;
} AstColumnRef;

typedef struct AstJoinClause
{
    C4Node node;
    AstTableRef *ref;
    bool not;
} AstJoinClause;

/* XXX: get rid of this? */
typedef struct AstQualifier
{
    C4Node node;
    C4Node *expr;
} AstQualifier;

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
    AST_OP_NEQ
} AstOperKind;

typedef struct AstOpExpr
{
    C4Node node;
    C4Node *lhs;
    C4Node *rhs;
    AstOperKind op_kind;
} AstOpExpr;

typedef struct AstVarExpr
{
    C4Node node;
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
    C4Node node;
    AstConstKind const_kind;
    char *value;
} AstConstExpr;

typedef enum AstAggKind
{
    AST_AGG_COUNT,
    AST_AGG_MAX,
    AST_AGG_MIN,
    AST_AGG_SUM
} AstAggKind;

typedef struct AstAggExpr
{
    C4Node node;
    AstAggKind agg_kind;
    C4Node *expr;
} AstAggExpr;

#endif  /* AST_H */
