#ifndef PARSER_H
#define PARSER_H

#include "parser/ast.h"

AstProgram *parse_str(const char *str, apr_pool_t *pool, ColInstance *col);

#endif  /* PARSER_H */
