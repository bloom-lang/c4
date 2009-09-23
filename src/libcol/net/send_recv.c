#include <apr_queue.h>
#include <apr_thread_proc.h>
#include <arpa/inet.h>
#include <limits.h>

#include "col-internal.h"
#include "router.h"
#include "net/network.h"
#include "net/send_recv.h"
#include "util/socket.h"
#include "util/strbuf.h"

#define MAX_TABLE_NAME 64
#define MAX_TUPLE_SIZE (1024 * 1024)

struct RecvThread
{
    ColInstance *col;
    apr_pool_t *pool;
    char *remote_loc;

    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;

    apr_socket_t *sock;
    char tbl_name[MAX_TABLE_NAME + 1];
    StrBuf *buf;
};

struct SendThread
{
    ColInstance *col;
    apr_pool_t *pool;
    char *remote_loc;

    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;

    apr_socket_t *sock;
    StrBuf *buf;

    apr_queue_t *queue;
};

typedef struct SendMessage
{
    TableDef *tbl_def;
    Tuple *tuple;
} SendMessage;

static void * APR_THREAD_FUNC recv_thread_main(apr_thread_t *thread, void *data);
static void * APR_THREAD_FUNC send_thread_main(apr_thread_t *thread, void *data);
static void create_send_socket(SendThread *st);
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
    rt->buf = sbuf_make(pool);

    return rt;
}

void
recv_thread_start(RecvThread *rt)
{
    apr_status_t s;

    s = apr_threadattr_create(&rt->thread_attr, rt->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_thread_create(&rt->thread, rt->thread_attr,
                          recv_thread_main, rt, rt->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

static void * APR_THREAD_FUNC
recv_thread_main(apr_thread_t *thread, void *data)
{
    RecvThread *rt = (RecvThread *) data;

    while (true)
    {
        bool is_eof;
        apr_uint32_t tbl_len;
        apr_uint32_t tuple_len;
        Tuple *tuple;
        TableDef *tbl_def;

        /* Length of the table name */
        tbl_len = socket_recv_uint32(rt->sock, &is_eof);
        if (is_eof)
            break;
        if (tbl_len > MAX_TABLE_NAME)
            FAIL();

        socket_recv_data(rt->sock, rt->tbl_name, tbl_len, &is_eof);
        if (is_eof)
            break;
        rt->tbl_name[tbl_len] = '\0';

        /* Length of the serialized tuple */
        tuple_len = socket_recv_uint32(rt->sock, &is_eof);
        if (is_eof)
            break;
        if (tuple_len > MAX_TUPLE_SIZE)
            FAIL();

        /* Read the serialized tuple value into buffer */
        sbuf_reset(rt->buf);
        sbuf_socket_recv(rt->buf, rt->sock, tuple_len, &is_eof);
        if (is_eof)
            break;

        /* Convert to in-memory tuple format, route tuple */
        tbl_def = cat_get_table(rt->col->cat, rt->tbl_name);    /* Thread-safety? */
        tuple = tuple_from_buf(rt->col, rt->buf, tbl_def);
        router_enqueue_tuple(rt->col->router, tuple, tbl_def);
        tuple_unpin(tuple);
    }

    /* We got EOF from the client socket, so cleanup and exit thread */
    network_cleanup_rt(rt->col->net, rt);
    apr_pool_destroy(rt->pool);
    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}

const char *
recv_thread_get_loc(RecvThread *rt)
{
    return rt->remote_loc;
}

/*
 * Note that we don't try to connect to the remote node here, because this
 * function is invoked by the router thread and hence shouldn't block.
 */
SendThread *
send_thread_make(ColInstance *col, const char *remote_loc, apr_pool_t *pool)
{
    apr_status_t s;
    SendThread *st;

    st = apr_pcalloc(pool, sizeof(*st));
    st->col = col;
    st->pool = pool;
    st->remote_loc = apr_pstrdup(pool, remote_loc);
    st->buf = sbuf_make(st->pool);

    s = apr_queue_create(&st->queue, 128, st->pool);
    if (s != APR_SUCCESS)
        FAIL();

    return st;
}

const char *
send_thread_get_loc(SendThread *st)
{
    return st->remote_loc;
}

void
send_thread_start(SendThread *st)
{
    apr_status_t s;

    s = apr_threadattr_create(&st->thread_attr, st->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_thread_create(&st->thread, st->thread_attr,
                          send_thread_main, st, st->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

static void * APR_THREAD_FUNC
send_thread_main(apr_thread_t *thread, void *data)
{
    SendThread *st = (SendThread *) data;

    create_send_socket(st);

    while (true)
    {
        apr_status_t s;
        SendMessage *msg;
        char *tbl_name;
        apr_size_t tbl_len;

        s = apr_queue_pop(st->queue, (void **) &msg);

        if (s == APR_EINTR)
            continue;
        if (s != APR_SUCCESS)
            FAIL_APR(s);

        tbl_name = msg->tbl_def->name;
#if 0
        printf("Sending tuple from %d => %s, table = %s\n",
               st->col->port, st->remote_loc, tbl_name);
#endif

        /* Send table name, prefixed with length */
        tbl_len = strlen(tbl_name);
        socket_send_uint32(st->sock, tbl_len);
        socket_send_data(st->sock, tbl_name, tbl_len);

        /* Convert in-memory tuple format into network format */
        sbuf_reset(st->buf);
        tuple_to_buf(msg->tuple, st->buf);

        socket_send_uint32(st->sock, st->buf->len);
        socket_send_data(st->sock, st->buf->data, st->buf->len);

        tuple_unpin(msg->tuple);
        ol_free(msg);
    }

    /* XXX: TODO */
    network_cleanup_st(st->col->net, st);
    apr_pool_destroy(st->pool);
    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}

/* XXX: The malloc() here is obnoxious. */
void
send_thread_enqueue(SendThread *st, Tuple *tuple, TableDef *tbl_def)
{
    SendMessage *msg;

    msg = ol_alloc(sizeof(*msg));
    msg->tuple = tuple;
    msg->tbl_def = tbl_def;
    tuple_pin(msg->tuple);

    while (true)
    {
        apr_status_t s = apr_queue_push(st->queue, msg);

        if (s == APR_EINTR)
            continue;
        if (s == APR_SUCCESS)
            break;

        FAIL();
    }
}

static void
create_send_socket(SendThread *st)
{
    apr_status_t s;
    char host[APRMAXHOSTLEN];
    int port_num;
    apr_sockaddr_t *addr;

    ASSERT(st->sock == NULL);

    parse_loc_spec(st->remote_loc, host, &port_num);

    s = apr_sockaddr_info_get(&addr, host, APR_INET, port_num, 0, st->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_socket_create(&st->sock, addr->family, SOCK_STREAM,
                          APR_PROTO_TCP, st->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_socket_connect(st->sock, addr);
    if (s != APR_SUCCESS)
        ERROR("Failed to connect to remote host @ %s", st->remote_loc);
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

    host_len = colon_ptr - p;
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
