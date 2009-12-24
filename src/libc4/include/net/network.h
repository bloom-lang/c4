#ifndef NETWORK_H
#define NETWORK_H

#include "net/send_recv.h"
#include "types/catalog.h"
#include "types/tuple.h"

typedef struct C4Network C4Network;

C4Network *network_make(C4Instance *c4, int port);
void network_start(C4Network *net);
void network_destroy(C4Network *net);

void network_cleanup_rt(C4Network *net, RecvThread *rt);
void network_cleanup_st(C4Network *net, SendThread *st);

void network_send(C4Network *net, Tuple *tuple, TableDef *tbl_def);

int network_get_port(C4Network *net);

#endif  /* NETWORK_H */
