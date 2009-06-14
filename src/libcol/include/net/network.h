#ifndef NETWORK_H
#define NETWORK_H

#include "types/tuple.h"

typedef struct ColNetwork ColNetwork;

ColNetwork *network_make(ColInstance *col, int port);
void network_destroy(ColNetwork *net);

void network_start(ColNetwork *net);
void network_send(ColNetwork *net, const char *loc, Tuple *tuple);
apr_pool_t *network_get_pool(ColNetwork *net);

#endif  /* NETWORK_H */
