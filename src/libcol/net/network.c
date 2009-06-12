#include "col-internal.h"
#include "net/network.h"

/*
 * Create a new instance of the network interface. "port" is the local TCP
 * port to listen on; 0 means to use an ephemeral port.
 */
ColNetwork *
network_make(ColInstance *col, int port)
{
    apr_status_t s;
    apr_pool_t *pool;
    ColNetwork *net;

    s = apr_pool_create(&pool, col->pool);
    if (s != APR_SUCCESS)
        return NULL;

    net = apr_palloc(pool, sizeof(*net));
    net->col = col;
    net->pool = pool;
    return net;
}

ColStatus
network_destroy(ColNetwork *net)
{
    apr_pool_destroy(net->pool);
    return COL_OK;
}

void
network_start(ColNetwork *net)
{
    ;
}
