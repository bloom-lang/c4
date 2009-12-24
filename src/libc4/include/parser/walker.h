#ifndef WALKER_H
#define WALKER_H

#include "parser/ast.h"

/*
 * A callback that is invoked on a node in an expression tree. If the
 * callback returns "false", the walk of the tree is halted immediately;
 * otherwise it continues in a depth-first fashion.
 */
typedef bool (*expr_callback)(C4Node *n, void *data);

bool expr_tree_walker(C4Node *n, expr_callback fn, void *data);

#endif  /* WALKER_H */
