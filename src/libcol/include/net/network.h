#ifndef NETWORK_H
#define NETWORK_H

typedef struct ColNetwork
{
    ColInstance *col;
    apr_pool_t *pool;
} ColNetwork;

ColNetwork *network_make(ColInstance *col, int port);
ColStatus network_destroy(ColNetwork *net);

void network_start(ColNetwork *net);

#endif  /* NETWORK_H */
