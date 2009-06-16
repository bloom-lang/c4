#include <arpa/inet.h>
#include <apr_queue.h>
#include <apr_thread_proc.h>
#include <apr_strings.h>
#include <limits.h>
#include <stdlib.h>

#include "col-internal.h"
#include "router.h"
#include "net/network.h"
#include "net/send_recv.h"
#include "util/socket.h"

#define MAX_MSG_SIZE    (1024 * 1024)

struct RecvThread
{
    ColInstance *col;
    apr_pool_t *pool;
    char *remote_loc;

    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;

    apr_socket_t *sock;
    char *buf;
    size_t buf_size;
};

struct SendThread
{
    ColInstance *col;
    apr_pool_t *pool;
    char *remote_loc;

    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;

    apr_socket_t *sock;
    apr_queue_t *queue;
};

static apr_status_t recv_thread_cleanup(void *data);
static void * APR_THREAD_FUNC recv_thread_main(apr_thread_t *thread, void *data);
static void * APR_THREAD_FUNC send_thread_main(apr_thread_t *thread, void *data);
static void establish_send_socket(SendThread *st);
static void parse_loc_spec(const char *loc_spec, char *host, int *port_p);

RecvThread *
recv_thread_make(ColInstance *col, apr_socket_t *sock, apr_pool_t *pool)
{
    RecvThread *rt;

    rt = apr_pcalloc(pool, sizeof(*rt));
    rt->col = col;
    rt->pool = pool;
    rt->sock = sock;
    rt->remote_loc = socket_get_remote_loc(sock, pool);
    rt->buf = ol_alloc(1024);
    rt->buf_size = 1024;

    apr_pool_cleanup_register(pool, rt, recv_thread_cleanup,
                              apr_pool_cleanup_null);

    return rt;
}

static apr_status_t
recv_thread_cleanup(void *data)
{
    RecvThread *rt = (RecvThread *) data;

    ol_free(rt->buf);
    return APR_SUCCESS;
}

void
recv_thread_start(RecvThread *rt)
{
    apr_status_t s;

    s = apr_threadattr_create(&rt->thread_attr, rt->pool);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_thread_create(&rt->thread, rt->thread_attr,
                          recv_thread_main, rt, rt->pool);
    if (s != APR_SUCCESS)
        FAIL();
}

static void * APR_THREAD_FUNC
recv_thread_main(apr_thread_t *thread, void *data)
{
    RecvThread *rt = (RecvThread *) data;

    while (true)
    {
        apr_uint32_t msg_len;
        Tuple *tuple;

        /* Length of the following message */
        msg_len = socket_recv_uint32(rt->sock);

        if (msg_len > MAX_MSG_SIZE)
            FAIL();

        if (msg_len > rt->buf_size)
        {
            rt->buf = ol_realloc(rt->buf, msg_len);
            rt->buf_size = msg_len;
        }

        /* Read the message itself */
        socket_recv_data(rt->sock, rt->buf, msg_len);

        /* Convert to tuple format, route tuple */
        tuple = tuple_from_buf(rt->buf, msg_len);
        router_enqueue(rt->col->router, tuple);
    }

    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}

const char *
recv_thread_get_loc(RecvThread *rt)
{
    return rt->remote_loc;
}

SendThread *
send_thread_make(ColInstance *col, const char *remote_loc, apr_pool_t *pool)
{
    apr_status_t s;
    SendThread *st;

    st = apr_pcalloc(pool, sizeof(*st));
    st->col = col;
    st->pool = pool;
    st->remote_loc = apr_pstrdup(pool, remote_loc);

    s = apr_queue_create(&st->queue, 128, st->pool);
    if (s != APR_SUCCESS)
        FAIL();

    return st;
}

void
send_thread_start(SendThread *st)
{
    apr_status_t s;

    s = apr_threadattr_create(&st->thread_attr, st->pool);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_thread_create(&st->thread, st->thread_attr,
                          send_thread_main, st, st->pool);
    if (s != APR_SUCCESS)
        FAIL();
}

static void * APR_THREAD_FUNC
send_thread_main(apr_thread_t *thread, void *data)
{
    SendThread *st = (SendThread *) data;

    establish_send_socket(st);

    while (true)
    {
        apr_status_t s;
        Tuple *tuple;

        s = apr_queue_pop(st->queue, (void **) &tuple);

        if (s == APR_EINTR)
            continue;
        if (s != APR_SUCCESS)
            FAIL();

        tuple_socket_send(tuple, st->sock);
    }

    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}

void
send_thread_enqueue(SendThread *st, Tuple *tuple)
{
    while (true)
    {
        apr_status_t s = apr_queue_push(st->queue, tuple);

        if (s == APR_EINTR)
            continue;
        if (s == APR_SUCCESS)
            break;

        FAIL();
    }
}

static void
establish_send_socket(SendThread *st)
{
    apr_status_t s;
    char host[APRMAXHOSTLEN];
    int port_num;
    apr_sockaddr_t *addr;

    ASSERT(st->sock == NULL);

    parse_loc_spec(st->remote_loc, host, &port_num);

    s = apr_sockaddr_info_get(&addr, host, APR_INET, port_num, 0, st->pool);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_socket_create(&st->sock, addr->family, SOCK_STREAM,
                          APR_PROTO_TCP, st->pool);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_socket_connect(st->sock, addr);
    if (s != APR_SUCCESS)
        FAIL();
}

static void
parse_loc_spec(const char *loc_spec, char *host, int *port_p)
{
    const char *p = loc_spec;
    char *colon_ptr;
    ptrdiff_t host_len;
    long raw_port;
    char *end_ptr;

    /* We only handle TCP for now */
    if (strncmp(p, "tcp:", 4) != 0)
        FAIL();

    p += 4;
    colon_ptr = rindex(p, ':');
    if (colon_ptr == NULL)
        FAIL();

    host_len = colon_ptr - p - 1;
    if (host_len <= 0 || host_len >= APRMAXHOSTLEN - 1)
        FAIL();

    memcpy(host, p, host_len);
    host[host_len] = '\0';

    p += host_len + 1;
    raw_port = strtol(p, &end_ptr, 10);
    if (p == end_ptr || raw_port <= 0 || raw_port > INT_MAX)
        FAIL();

    *port_p = (int) raw_port;
}
