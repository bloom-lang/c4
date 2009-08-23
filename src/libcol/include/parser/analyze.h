#ifndef ANALYZE_H
#define ANALYZE_H

#include "parser/ast.h"
#include "parser/parser.h"

ParseResult *analyze_ast(AstProgram *program, apr_pool_t *pool);

#endif  /* ANALYZE_H */
