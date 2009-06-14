#ifndef NETWORK_H
#define NETWORK_H

#include <apr_network_io.h>
#include <apr_thread_proc.h>

typedef struct ColNetwork
{
    ColInstance *col;
    apr_pool_t *pool;

    /* Thread info for server thread */
    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;

    /* Socket info */
    apr_socket_t *sock;
} ColNetwork;

ColNetwork *network_make(ColInstance *col, int port);
void network_destroy(ColNetwork *net);

void network_start(ColNetwork *net);

#endif  /* NETWORK_H */