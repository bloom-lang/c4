#ifndef ROUTER_H
#define ROUTER_H

#include "c4-api.h"     /* XXX: Wrong */
#include "operator/operator.h"
#include "planner/planner.h"
#include "types/tuple.h"

typedef struct C4Router C4Router;

C4Router *router_make(C4Runtime *c4);
void router_main_loop(C4Router *router);

/* Public APIs */
void runtime_enqueue_program(C4Runtime *c4, const char *src);
void runtime_enqueue_tuple(C4Runtime *c4, Tuple *tuple,
                           TableDef *tbl_def);
char *runtime_enqueue_dump_table(C4Runtime *c4, const char *tbl_name,
                                 apr_pool_t *pool);
void runtime_enqueue_callback(C4Runtime *c4, const char *tbl_name,
                              C4TableCallback callback, void *data);
void runtime_enqueue_shutdown(C4Runtime *c4);

/* Internal APIs: XXX: clearer naming */
void router_do_fixpoint(C4Router *router);
void router_install_tuple(C4Router *router, Tuple *tuple,
                          TableDef *tbl_def, bool check_remote);
void router_enqueue_internal(C4Router *router, Tuple *tuple,
                             TableDef *tbl_def);
void router_enqueue_net(C4Router *router, Tuple *tuple,
                        TableDef *tbl_def);

OpChainList *router_get_opchain_list(C4Router *router, const char *tbl_name);
void router_add_op_chain(C4Router *router, OpChain *op_chain);

#endif  /* ROUTER_H */
