#ifndef PARSER_H
#define PARSER_H

#include "parser/ast.h"

typedef struct RangeTblEntry
{
    ColNode node;
    int   idx;          /* Unique within a single ParseResult */
    char *rel_name;     /* Not unique within a ParseResult (self-joins) */
    List *vars;
} RangeTblEntry;

typedef struct ParseResult
{
    ColNode node;
    AstProgram *ast;
    List *range_tbl;
} ParseResult;

ParseResult *parse_str(const char *str, apr_pool_t *pool, ColInstance *col);

#endif  /* PARSER_H */
