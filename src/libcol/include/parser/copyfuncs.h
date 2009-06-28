#ifndef COPYFUNCS_H
#define COPYFUNCS_H

#include "parser/ast.h"

void *node_deep_copy(void *n, apr_pool_t *pool);

#endif  /* COPYFUNCS_H */
