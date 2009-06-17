#ifndef ANALYZE_H
#define ANALYZE_H

#include "parser/ast.h"

void analyze_ast(AstProgram *program, apr_pool_t *pool);

#endif  /* ANALYZE_H */
