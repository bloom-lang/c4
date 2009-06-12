#include "col-internal.h"
#include "net/network.h"

static void * APR_THREAD_FUNC network_thread_start(apr_thread_t *thread, void *data);

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

    net = apr_pcalloc(pool, sizeof(*net));
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

ColStatus
network_start(ColNetwork *net)
{
    apr_status_t s;

    s = apr_threadattr_create(&net->thread_attr, net->pool);
    if (s != APR_SUCCESS)
        return COL_ERROR;

    s = apr_thread_create(&net->thread, net->thread_attr,
                          network_thread_start, net, net->pool);
    if (s != APR_SUCCESS)
        return COL_ERROR;

    return COL_OK;
}

static void * APR_THREAD_FUNC
network_thread_start(apr_thread_t *thread, void *data)
{
    while (true)
        ;

    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}
