#include <apr_network_io.h>
#include <apr_strings.h>
#include <apr_thread_proc.h>

#include "col-internal.h"
#include "net/connection.h"

struct ColConnection
{
    ColInstance *col;
    apr_pool_t *pool;

    ConnectionState state;
    apr_socket_t *sock;
    char *remote_loc;
};

static char *get_sock_remote_loc(apr_socket_t *sock, apr_pool_t *pool);

ColConnection *
connection_make(apr_socket_t *sock, ColInstance *col, apr_pool_t *pool)
{
    ColConnection *conn;

    conn = apr_pcalloc(pool, sizeof(*conn));
    conn->col = col;
    conn->pool = pool;
    conn->sock = sock;
    conn->state = COL_CONNECTED;
    conn->remote_loc = get_sock_remote_loc(conn->sock, conn->pool);

    return conn;
}

void
connection_destroy(ColConnection *conn)
{
    ;
}

void
connection_send(ColConnection *conn, Tuple *tuple)
{
    ;
}

ConnectionState
connection_get_state(ColConnection *conn)
{
    return conn->state;
}

char *
connection_get_remote_loc(ColConnection *conn)
{
    return conn->remote_loc;
}

static char *
get_sock_remote_loc(apr_socket_t *sock, apr_pool_t *pool)
{
    apr_status_t s;
    apr_sockaddr_t *addr;
    char *ip;

    s = apr_socket_addr_get(&addr, APR_REMOTE, sock);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_sockaddr_ip_get(&ip, addr);
    if (s != APR_SUCCESS)
        FAIL();

    return apr_psprintf(pool, "tcp:%s:%us", ip, addr->port);
}
