#ifndef ANALYZE_H
#define ANALYZE_H

#include "parser/ast.h"
#include "types/schema.h"

void analyze_ast(AstProgram *program, apr_pool_t *pool);
DataType expr_get_type(ColNode *node);

#endif  /* ANALYZE_H */
