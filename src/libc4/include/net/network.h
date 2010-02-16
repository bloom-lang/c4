#ifndef NETWORK_H
#define NETWORK_H

#include "types/catalog.h"
#include "types/tuple.h"

typedef struct C4Network C4Network;

C4Network *network_make(C4Runtime *c4, int port);
void network_destroy(C4Network *net);

bool network_poll(C4Network *net, apr_interval_time_t timeout);
void network_wakeup(C4Network *net);
void network_send(C4Network *net, Tuple *tuple, TableDef *tbl_def);

int network_get_port(C4Network *net);

#endif  /* NETWORK_H */
