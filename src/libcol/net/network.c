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
        FAIL();

    net = apr_pcalloc(pool, sizeof(*net));
    net->col = col;
    net->pool = pool;

    s = apr_sockaddr_info_get(&net->addr, NULL, APR_INET, port, 0, net->pool);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_socket_create(&net->sock, net->addr->family,
                          SOCK_STREAM, APR_PROTO_TCP, net->pool);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_socket_opt_set(net->sock, APR_SO_REUSEADDR, 1);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_socket_bind(net->sock, net->addr);
    if (s != APR_SUCCESS)
        FAIL();

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
    ColNetwork *net = (ColNetwork *) data;
    apr_status_t s;

    s = apr_socket_listen(net->sock, SOMAXCONN);
    if (s != APR_SUCCESS)
        FAIL();

    while (true)
    {
        apr_socket_t *client_sock;

        s = apr_socket_accept(&client_sock, net->sock, net->pool);
        if (s != APR_SUCCESS)
            FAIL();
    }

    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}
