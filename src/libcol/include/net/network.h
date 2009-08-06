#ifndef NETWORK_H
#define NETWORK_H

#include "net/send_recv.h"
#include "types/tuple.h"

typedef struct ColNetwork ColNetwork;

ColNetwork *network_make(ColInstance *col, int port);
void network_start(ColNetwork *net);
void network_destroy(ColNetwork *net);

void network_cleanup_rt(ColNetwork *net, RecvThread *rt);
void network_cleanup_st(ColNetwork *net, SendThread *st);

void network_send(ColNetwork *net, Datum loc_spec,
                  const char *tbl_name, Tuple *tuple);
int network_get_port(ColNetwork *net);

#endif  /* NETWORK_H */
