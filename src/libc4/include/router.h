#ifndef ROUTER_H
#define ROUTER_H

#include "c4-api-callback.h"
#include "operator/operator.h"
#include "planner/planner.h"
#include "types/tuple.h"

typedef struct C4Router C4Router;

C4Router *router_make(C4Runtime *c4);
void router_main_loop(C4Router *router);

/* Public APIs */
char *runtime_enqueue_dump_table(C4Runtime *c4, const char *tbl_name,
                                 apr_pool_t *pool);

/* Internal APIs: XXX: clearer naming */
void router_insert_tuple(C4Router *router, Tuple *tuple,
                         TableDef *tbl_def, bool check_remote);
void router_delete_tuple(C4Router *router, Tuple *tuple, TableDef *tbl_def);
void router_enqueue_internal(C4Router *router, Tuple *tuple, TableDef *tbl_def);

OpChainList *router_get_opchain_list(C4Router *router, const char *tbl_name);
void router_add_op_chain(C4Router *router, OpChain *op_chain);
bool router_is_deleting(C4Router *router);

#endif  /* ROUTER_H */
