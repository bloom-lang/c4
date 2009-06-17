#ifndef PARSER_H
#define PARSER_H

#include "parser/ast.h"

AstProgram *parse_str(ColInstance *col, const char *str, apr_pool_t *pool);

#endif  /* PARSER_H */
