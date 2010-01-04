#ifndef ANALYZE_H
#define ANALYZE_H

#include "parser/ast.h"
#include "types/schema.h"

void analyze_ast(AstProgram *program, apr_pool_t *pool, C4Runtime *c4);
DataType expr_get_type(C4Node *node);

#endif  /* ANALYZE_H */
