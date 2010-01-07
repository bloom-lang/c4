#ifndef ROUTER_H
#define ROUTER_H

#include <apr_queue.h>

#include "operator/operator.h"
#include "planner/planner.h"
#include "types/tuple.h"

typedef struct C4Router C4Router;

C4Router *router_make(C4Runtime *c4, apr_queue_t *queue);
void router_main_loop(C4Router *router);
apr_queue_t *router_get_queue(C4Router *router);

void router_enqueue_program(apr_queue_t *queue, const char *src);
void router_enqueue_tuple(apr_queue_t *queue, Tuple *tuple,
                          TableDef *tbl_def);

char *router_enqueue_dump_table(apr_queue_t *queue, const char *table,
                                apr_pool_t *pool);

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
