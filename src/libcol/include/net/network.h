#ifndef NETWORK_H
#define NETWORK_H

typedef struct ColNetwork ColNetwork;

ColNetwork *network_make(ColInstance *col, int port);
void network_destroy(ColNetwork *net);

void network_start(ColNetwork *net);

#endif  /* NETWORK_H */
