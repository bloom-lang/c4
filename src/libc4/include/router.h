#ifndef ROUTER_H
#define ROUTER_H

#include "operator/operator.h"
#include "planner/planner.h"
#include "types/tuple.h"

typedef struct ColRouter ColRouter;

ColRouter *router_make(ColInstance *col);
void router_destroy(ColRouter *router);

void router_start(ColRouter *router);

void router_enqueue_program(ColRouter *router, const char *src);
void router_enqueue_tuple(ColRouter *router, Tuple *tuple,
                          TableDef *tbl_def);

/* Internal APIs: XXX: clearer naming */
void router_install_tuple(ColRouter *router, Tuple *tuple,
                          TableDef *tbl_def);
void router_enqueue_internal(ColRouter *router, Tuple *tuple,
                             TableDef *tbl_def);
void router_enqueue_net(ColRouter *router, Tuple *tuple,
                        TableDef *tbl_def);

OpChainList *router_get_opchain_list(ColRouter *router, const char *tbl_name);
void router_add_op_chain(ColRouter *router, OpChain *op_chain);

#endif  /* ROUTER_H */
