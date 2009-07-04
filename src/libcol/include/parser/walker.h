#ifndef WALKER_H
#define WALKER_H

#include "parser/ast.h"

typedef void (*expr_callback)(AstNode *n, void *data);

void expr_tree_walker(AstNode *n, expr_callback fn, void *data);

#endif  /* WALKER_H */
