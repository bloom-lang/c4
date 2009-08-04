#ifndef NETWORK_H
#define NETWORK_H

#include "types/tuple.h"

typedef struct ColNetwork ColNetwork;

ColNetwork *network_make(ColInstance *col, int port);
void network_start(ColNetwork *net);
void network_destroy(ColNetwork *net);

void network_send(ColNetwork *net, Datum addr,
                  const char *tbl_name, Tuple *tuple);

#endif  /* NETWORK_H */
