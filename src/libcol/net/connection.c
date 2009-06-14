#include <apr_network_io.h>
#include <apr_strings.h>
#include <apr_thread_proc.h>

#include "col-internal.h"
#include "net/connection.h"

struct ColConnection
{
    ColInstance *col;
    ColNetwork *net;
    apr_pool_t *pool;

    ConnectionState state;
    apr_socket_t *sock;
    char *remote_loc;
};

ColConnection *
connection_make(apr_socket_t *sock, const char *remote_loc,
                ColNetwork *net, ColInstance *col)
{
    apr_status_t s;
    apr_pool_t *pool;
    ColConnection *conn;

    s = apr_pool_create(&pool, network_get_pool(net));
    if (s != APR_SUCCESS)
        FAIL();

    conn = apr_pcalloc(pool, sizeof(*conn));
    conn->col = col;
    conn->net = net;
    conn->pool = pool;
    conn->sock = sock;
    conn->remote_loc = apr_pstrdup(conn->pool, remote_loc);
    conn->state = COL_CONNECTED;

    return conn;
}

ConnectionState
connection_get_state(ColConnection *conn)
{
    return conn->state;
}

void
connection_send(ColConnection *conn, Tuple *tuple)
{
    ;
}
