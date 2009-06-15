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
    char *buf;

    /* Thread info for connection thread */
    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;
};

#define CONN_BUFFER_SIZE 1024

static void * APR_THREAD_FUNC conn_thread_start(apr_thread_t *thread, void *data);
static char *get_sock_remote_loc(apr_socket_t *sock, apr_pool_t *pool);
static void start_connection(ColConnection *conn);
static void parse_loc_spec(const char *loc_spec, char *host, int *port_p);

/*
 * Wrap an already-existing client socket in a ColConnection.
 */
ColConnection *
connection_make(apr_socket_t *sock, ColInstance *col, apr_pool_t *pool)
{
    ColConnection *conn;
    apr_status_t s;

    conn = apr_pcalloc(pool, sizeof(*conn));
    conn->col = col;
    conn->pool = pool;
    conn->sock = sock;
    conn->state = COL_CONNECTED;
    conn->remote_loc = get_sock_remote_loc(conn->sock, conn->pool);
    conn->buf = apr_palloc(conn->pool, CONN_BUFFER_SIZE);

    /* Create and start per-connection thread */
    s = apr_threadattr_create(&conn->thread_attr, conn->pool);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_thread_create(&conn->thread, conn->thread_attr,
                          conn_thread_start, conn, conn->pool);
    if (s != APR_SUCCESS)
        FAIL();

    return conn;
}

/*
 * Create a connection to a new host.
 */
ColConnection *
connection_new_connect(const char *remote_loc, ColInstance *col,
                       apr_pool_t *net_pool)
{
    apr_status_t s;
    apr_pool_t *pool;
    ColConnection *conn;

    s = apr_pool_create(&pool, net_pool);
    if (s != APR_SUCCESS)
        FAIL();

    conn = apr_pcalloc(pool, sizeof(*conn));
    conn->col = col;
    conn->pool = pool;
    conn->state = COL_NEW;
    conn->remote_loc = apr_pstrdup(conn->pool, remote_loc);
    conn->buf = apr_palloc(conn->pool, CONN_BUFFER_SIZE);

    /* Create and start per-connection thread */
    s = apr_threadattr_create(&conn->thread_attr, conn->pool);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_thread_create(&conn->thread, conn->thread_attr,
                          conn_thread_start, conn, conn->pool);
    if (s != APR_SUCCESS)
        FAIL();

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

static void * APR_THREAD_FUNC
conn_thread_start(apr_thread_t *thread, void *data)
{
    ColConnection *conn = (ColConnection *) data;

    if (conn->state == COL_NEW)
        start_connection(conn);

    while (true)
    {
        apr_status_t s;
        apr_size_t len;

        /* Read the length of the next packet */
        len = 4;
        s = apr_socket_recv(conn->sock, conn->buf, &len);
        if (s != APR_SUCCESS)
            FAIL();
        if (len != 4)
            FAIL();
    }

    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
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

static void
start_connection(ColConnection *conn)
{
    char host[APRMAXHOSTLEN];
    int port_num;

    parse_loc_spec(conn->remote_loc, host, &port_num);
}

static void
parse_loc_spec(const char *loc_spec, char *host, int *port_p)
{
    ;
}
