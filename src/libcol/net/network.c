#include <apr_hash.h>
#include <apr_network_io.h>
#include <apr_thread_proc.h>

#include "col-internal.h"
#include "net/network.h"
#include "net/send_recv.h"

struct ColNetwork
{
    ColInstance *col;
    apr_pool_t *pool;

    /* Thread info for server socket thread */
    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;

    /* Server socket info */
    apr_socket_t *sock;
    apr_sockaddr_t *local_addr;

    apr_hash_t *recv_tbl;
    apr_hash_t *send_tbl;

    /*
     * XXX: Used to convert loc spec datums into C-style strings. This is a
     * hack.
     */
    StrBuf *tmp_buf;
};

static void * APR_THREAD_FUNC network_thread_main(apr_thread_t *thread, void *data);
static void new_recv_thread(ColNetwork *net, apr_socket_t *sock, apr_pool_t *recv_pool);
static SendThread *get_send_thread(ColNetwork *net, Tuple *tuple, TableDef *tbl_def);

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
    apr_sockaddr_t *addr;

    pool = make_subpool(col->pool);
    net = apr_pcalloc(pool, sizeof(*net));
    net->col = col;
    net->pool = pool;
    net->recv_tbl = apr_hash_make(net->pool);
    net->send_tbl = apr_hash_make(net->pool);
    net->tmp_buf = sbuf_make(net->pool);

    s = apr_sockaddr_info_get(&addr, NULL, APR_INET, port, 0, net->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_socket_create(&net->sock, addr->family,
                          SOCK_STREAM, APR_PROTO_TCP, net->pool);
    if (s != APR_SUCCESS)
        ERROR("Failed to create local TCP socket, port %d", port);

    s = apr_socket_opt_set(net->sock, APR_SO_REUSEADDR, 1);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_socket_bind(net->sock, addr);
    if (s != APR_SUCCESS)
        ERROR("Failed to bind to local TCP socket, port %d", port);

    s = apr_socket_addr_get(&net->local_addr, APR_LOCAL, net->sock);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return net;
}

void
network_destroy(ColNetwork *net)
{
    apr_pool_destroy(net->pool);
}

void
network_start(ColNetwork *net)
{
    apr_status_t s;

    s = apr_threadattr_create(&net->thread_attr, net->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_thread_create(&net->thread, net->thread_attr,
                          network_thread_main, net, net->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

int
network_get_port(ColNetwork *net)
{
    return net->local_addr->port;
}

void
network_cleanup_rt(ColNetwork *net, RecvThread *rt)
{
    apr_hash_set(net->recv_tbl, recv_thread_get_loc(rt),
                 APR_HASH_KEY_STRING, NULL);
}

void
network_cleanup_st(ColNetwork *net, SendThread *st)
{
    apr_hash_set(net->send_tbl, send_thread_get_loc(st),
                 APR_HASH_KEY_STRING, NULL);
}

static void * APR_THREAD_FUNC
network_thread_main(apr_thread_t *thread, void *data)
{
    ColNetwork *net = (ColNetwork *) data;
    apr_status_t s;

    s = apr_socket_listen(net->sock, SOMAXCONN);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    while (true)
    {
        apr_pool_t *recv_pool;
        apr_socket_t *client_sock;

        /*
         * We need to preallocate the RecvThread's pool, so that we can
         * allocate the client socket in it.
         */
        recv_pool = make_subpool(net->pool);

        s = apr_socket_accept(&client_sock, net->sock, recv_pool);
        if (s != APR_SUCCESS)
            FAIL_APR(s);

        new_recv_thread(net, client_sock, recv_pool);
    }

    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}

/* XXX: check if there's an existing recv thread at socket loc? */
static void
new_recv_thread(ColNetwork *net, apr_socket_t *sock, apr_pool_t *recv_pool)
{
    RecvThread *rt;

    rt = recv_thread_make(net->col, sock, recv_pool);
    apr_hash_set(net->recv_tbl, recv_thread_get_loc(rt),
                 APR_HASH_KEY_STRING, rt);
    recv_thread_start(rt);
}

void
network_send(ColNetwork *net, Tuple *tuple, TableDef *tbl_def)
{
    SendThread *st;

    ASSERT(tbl_def->ls_colno != -1);
    st = get_send_thread(net, tuple, tbl_def);
    send_thread_enqueue(st, tuple, tbl_def);
}

static SendThread *
get_send_thread(ColNetwork *net, Tuple *tuple, TableDef *tbl_def)
{
    SendThread *st;
    Datum loc_spec;
    char *tmp_loc_str;

    loc_spec = tuple_get_val(tuple, tbl_def->ls_colno);
    /*
     * XXX: Convert tuple address in Datum format into a C-style
     * string. This is hacky.
     */
    sbuf_reset(net->tmp_buf);
    datum_to_str(loc_spec, TYPE_STRING, net->tmp_buf);
    sbuf_append_char(net->tmp_buf, '\0');
    tmp_loc_str = net->tmp_buf->data;

    st = apr_hash_get(net->send_tbl, tmp_loc_str, APR_HASH_KEY_STRING);
    if (st == NULL)
    {
        st = send_thread_make(net->col, tmp_loc_str,
                              make_subpool(net->pool));
        apr_hash_set(net->send_tbl, send_thread_get_loc(st),
                     APR_HASH_KEY_STRING, st);
        send_thread_start(st);
    }

    return st;
}
