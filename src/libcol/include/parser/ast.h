#ifndef AST_H
#define AST_H

#include "util/list.h"

typedef struct AstProgram
{
    char *name;
    List *clauses;
} AstProgram;

typedef enum AstClauseType
{
    AST_DEFINE,
    AST_RULE,
    AST_FACT
} AstClauseType;

typedef struct AstClause
{
    AstClauseType type;
} AstClause;

typedef struct AstDefine
{
    AstClause parent;
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
    char *name;
    AstHashVariant hash_variant;
    List *cols;
} AstTableRef;

typedef struct AstColumnRef
{
    bool has_loc_spec;
    char *name;
} AstColumnRef;

typedef struct AstRule
{
    AstClause parent;
    bool is_delete;
    AstTableRef *head;
    List *body;
} AstRule;

typedef struct AstFact
{
    AstClause parent;
    AstTableRef *head;
} AstFact;

#endif  /* AST_H */
