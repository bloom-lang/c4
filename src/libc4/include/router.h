#ifndef ROUTER_H
#define ROUTER_H

#include "operator/operator.h"
#include "planner/planner.h"
#include "types/tuple.h"

typedef struct C4Router C4Router;

C4Router *router_make(C4Instance *c4);
void router_destroy(C4Router *router);

void router_start(C4Router *router);

void router_enqueue_program(C4Router *router, const char *src);
void router_enqueue_tuple(C4Router *router, Tuple *tuple,
                          TableDef *tbl_def);

/* Internal APIs: XXX: clearer naming */
void router_install_tuple(C4Router *router, Tuple *tuple,
                          TableDef *tbl_def);
void router_enqueue_internal(C4Router *router, Tuple *tuple,
                             TableDef *tbl_def);
void router_enqueue_net(C4Router *router, Tuple *tuple,
                        TableDef *tbl_def);

OpChainList *router_get_opchain_list(C4Router *router, const char *tbl_name);
void router_add_op_chain(C4Router *router, OpChain *op_chain);

#endif  /* ROUTER_H */
