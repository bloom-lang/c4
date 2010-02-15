#include <apr_network_io.h>
#include <apr_poll.h>
#include <apr_thread_proc.h>

#include "c4-internal.h"
#include "net/network.h"
#include "router.h"
#include "util/hash.h"
#include "util/socket.h"
#include "util/strbuf.h"
#include "util/tuple_buf.h"

struct C4Network
{
    C4Runtime *c4;
    apr_pool_t *pool;

    /* Server socket info */
    apr_pollfd_t *pollfd;
    apr_socket_t *serv_sock;
    apr_sockaddr_t *local_addr;

    apr_pollset_t *pollset;

    /*
     * Map from location specifiers => ClientState. Note that this is
     * imprecise: we might get an incoming connection from a host
     * whose loc spec already exists in the table. In that case, we
     * don't bother adding the new client to the table, although we
     * allow the incoming connection to proceed.
     */
    c4_hash_t *client_tbl;
};

/* XXX: Rename */
typedef enum RecvState
{
    RECV_TABLE_NAME_LEN,
    RECV_TABLE_NAME,
    RECV_TUPLE_LEN,
    RECV_TUPLE
} RecvState;

typedef enum SendState
{
    SEND_IDLE,
    SEND_HEADER,
    SEND_TUPLE
} SendState;

typedef struct ClientState
{
    apr_pool_t *pool;
    C4Runtime *c4;
    Datum loc_spec;       /* Pre-formatted for hash table lookups */
    char *loc_spec_str;
    apr_sockaddr_t *remote_addr;

    bool connected;
    apr_socket_t *sock;
    apr_pollfd_t *pollfd;

    /* Receive-side state: incoming data from client */
    RecvState recv_state;
    StrBuf *recv_name_buf;
    StrBuf *recv_tuple_buf;
    apr_uint16_t tbl_name_len;
    apr_uint32_t tuple_len;

    /* Send-side state: outgoing data to client */
    SendState send_state;
    StrBuf *send_header_buf;    /* Headers for current tuple */
    StrBuf *send_tuple_buf;     /* Current outgoing tuple */
    TupleBuf *pending_tuples;   /* Future outgoing tuples */
} ClientState;

#define POLLSET_SIZE 64

static apr_status_t network_cleanup(void *data);
static apr_socket_t *server_sock_make(int port, apr_pool_t *pool);
static apr_pollfd_t *pollfd_make(apr_pool_t *pool, apr_socket_t *sock,
                                 apr_int16_t reqevents, void *data);
static unsigned int client_tbl_hash(const char *key, int klen, void *user_data);
static bool client_tbl_cmp(const void *k1, const void *k2, int klen,
                           void *user_data);
static void accept_new_client(C4Network *net);
static void update_client_state(const apr_pollfd_t *fd);
static void update_recv_state(ClientState *client);
static void update_send_state(ClientState *client);
static void deserialize_tuple(ClientState *client);
static ClientState *client_make(C4Network *net);
static apr_status_t client_cleanup(void *data);
static ClientState *get_client_for_loc_spec(C4Network *net, Tuple *tuple,
                                            TableDef *tbl_def);
static ClientState *connect_new_client(C4Network *net, Datum loc_spec);
static void client_try_connect(ClientState *client);
static void update_client_interest(ClientState *client, int reqevents);
static apr_socket_t *create_send_socket(ClientState *client,
                                        apr_sockaddr_t **remote_addr);
static void parse_loc_spec(const char *loc_spec, char *host, int *port_p);

/*
 * Create a new instance of the network interface. "port" is the local TCP
 * port to listen on; 0 means to use an ephemeral port.
 */
C4Network *
network_make(C4Runtime *c4, int port)
{
    apr_status_t s;
    C4Network *net;

    net = apr_pcalloc(c4->pool, sizeof(*net));
    net->c4 = c4;
    net->pool = c4->pool;
    net->client_tbl = c4_hash_make(net->pool, sizeof(Datum), NULL,
                                   client_tbl_hash, client_tbl_cmp);
    net->serv_sock = server_sock_make(port, net->pool);

    s = apr_socket_addr_get(&net->local_addr, APR_LOCAL, net->serv_sock);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_pollset_create(&net->pollset, POLLSET_SIZE, net->pool,
                           APR_POLLSET_WAKEABLE | APR_POLLSET_NOCOPY);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    printf("Using poll method: %s\n", apr_pollset_method_name(net->pollset));

    net->pollfd = pollfd_make(net->pool, net->serv_sock, APR_POLLIN, NULL);
    s = apr_pollset_add(net->pollset, net->pollfd);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    apr_pool_cleanup_register(c4->pool, net, network_cleanup,
                              apr_pool_cleanup_null);

    return net;
}

static apr_status_t
network_cleanup(void *data)
{
    C4Network *net = (C4Network *) data;

    /* Sanity check: no more clients in table */
    if (c4_hash_count(net->client_tbl) != 0)
        FAIL();

    return APR_SUCCESS;
}

static apr_socket_t *
server_sock_make(int port, apr_pool_t *pool)
{
    apr_status_t s;
    apr_sockaddr_t *addr;
    apr_socket_t *serv_sock;

    s = apr_sockaddr_info_get(&addr, NULL, APR_INET, port, 0, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_socket_create(&serv_sock, addr->family,
                          SOCK_STREAM, APR_PROTO_TCP, pool);
    if (s != APR_SUCCESS)
        ERROR("Failed to create local TCP socket, port %d", port);

    s = apr_socket_opt_set(serv_sock, APR_SO_REUSEADDR, 1);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    socket_set_non_block(serv_sock);

    s = apr_socket_bind(serv_sock, addr);
    if (s != APR_SUCCESS)
        ERROR("Failed to bind to local TCP socket, port %d", port);

    s = apr_socket_listen(serv_sock, SOMAXCONN);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return serv_sock;
}

static apr_pollfd_t *
pollfd_make(apr_pool_t *pool, apr_socket_t *sock,
            apr_int16_t reqevents, void *data)
{
    apr_pollfd_t *result;

    result = apr_palloc(pool, sizeof(*result));
    result->p = pool;
    result->desc.s = sock;
    result->desc_type = APR_POLL_SOCKET;
    result->reqevents = reqevents;
    result->rtnevents = 0;
    result->client_data = data;

    return result;
}

static unsigned int
client_tbl_hash(const char *key, int klen, void *user_data)
{
    Datum d;

    ASSERT(klen == sizeof(Datum));
    /* Ugly workaround: C99 does not allow casting pointer to union type */
    d.s = (C4String *) key;
    return string_hash(d);
}

static bool
client_tbl_cmp(const void *k1, const void *k2, int klen, void *user_data)
{
    Datum d1;
    Datum d2;

    ASSERT(klen == sizeof(Datum));
    d1.s = (C4String *) k1;
    d2.s = (C4String *) k2;
    return string_equal(d1, d2);
}

int
network_get_port(C4Network *net)
{
    return net->local_addr->port;
}

/*
 * Block for a new event: network activity, network_wakeup(), or timeout. We
 * only return to the caller if we see a wakeup(); if any incoming tuples
 * arrive, we send them to the router and then compute a fixpoint ourselves.
 */
void
network_poll(C4Network *net, apr_interval_time_t timeout)
{
    while (true)
    {
        apr_status_t s;
        apr_int32_t num;
        const apr_pollfd_t *descriptors;
        int i;

        s = apr_pollset_poll(net->pollset, timeout, &num, &descriptors);
        if (s == APR_EINTR)
            return;         /* network_wakeup() was called */
        if (s == APR_TIMEUP)
            return;         /* timeout expired */
        if (s != APR_SUCCESS)
            FAIL_APR(s);

        for (i = 0; i < num; i++)
        {
            if (descriptors[i].desc.s == net->serv_sock)
                accept_new_client(net);
            else
                update_client_state(&descriptors[i]);
        }
    }
}

/*
 * Interrupt a blocking network_poll(). This is typically invoked by a client
 * thread.
 */
void
network_wakeup(C4Network *net)
{
    apr_status_t s;

    s = apr_pollset_wakeup(net->pollset);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

static void
accept_new_client(C4Network *net)
{
    ClientState *client;
    apr_status_t s;

    client = client_make(net);

    s = apr_socket_accept(&client->sock, net->serv_sock, client->pool);
    if (s != APR_SUCCESS)
    {
        apr_pool_destroy(client->pool);
        return;
    }

    socket_set_non_block(client->sock);
    client->connected = true;
    client->loc_spec_str = socket_get_remote_loc(client->sock, client->pool);
    client->remote_addr = socket_get_remote_addr(client->sock);
    client->pollfd = pollfd_make(client->pool, client->sock,
                                 APR_POLLIN, client);
    s = apr_pollset_add(net->pollset, client->pollfd);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    client->loc_spec = string_from_str(client->loc_spec_str);
    pool_track_datum(client->pool, client->loc_spec, TYPE_STRING);
    /*
     * Enter the new client's loc_spec into the client_table. If
     * there's already a ClientState for the loc spec, don't try to
     * replace it.
     */
    c4_hash_set_if_new(net->client_tbl, client->loc_spec.s, client, NULL);
}

static ClientState *
client_make(C4Network *net)
{
    ClientState *client;
    apr_pool_t *client_pool;

    client_pool = make_subpool(net->pool);
    client = apr_pcalloc(client_pool, sizeof(*client));
    client->pool = client_pool;
    client->c4 = net->c4;
    client->connected = false;
    client->recv_state = RECV_TABLE_NAME_LEN;
    client->recv_name_buf = sbuf_make(client->pool);
    client->recv_tuple_buf = sbuf_make(client->pool);
    client->send_state = SEND_IDLE;
    client->send_header_buf = sbuf_make(client->pool);
    client->send_tuple_buf = sbuf_make(client->pool);
    client->pending_tuples = tuple_buf_make(64, client->pool);

    apr_pool_pre_cleanup_register(client->pool, client, client_cleanup);

    return client;
}

/*
 * Note that this is registered as a pre_cleanup, so it will be invoked before
 * other cleanup functions for the client's pool. This is important, because the
 * socket cleanup function will close the socket, which would cause
 * apr_pollset_remove() to fail.
 */
static apr_status_t
client_cleanup(void *data)
{
    ClientState *client = (ClientState *) data;
    C4Network *net = client->c4->net;
    apr_status_t s;

    if (!tuple_buf_is_empty(client->pending_tuples))
        c4_log(client->c4, "Destroying client @ %s with %d unsent messages",
               client->loc_spec_str, tuple_buf_size(client->pending_tuples));

    s = apr_pollset_remove(net->pollset, client->pollfd);
    if (s != APR_SUCCESS)
        c4_warn_apr(client->c4, s, "Failed to remove client @ %s from pollset",
                    client->loc_spec_str);

    s = apr_socket_close(client->sock);
    if (s != APR_SUCCESS)
        c4_warn_apr(client->c4, s, "Close on client socket @ %s failed",
                    client->loc_spec_str);

    c4_hash_set(net->client_tbl, client->loc_spec.s, NULL);

    return APR_SUCCESS;
}

static void
update_client_state(const apr_pollfd_t *pollfd)
{
    ClientState *client = (ClientState *) pollfd->client_data;

    /* Data available to be read from client? */
    if (pollfd->rtnevents & APR_POLLIN)
        update_recv_state(client);

    /* Space available to write to client? */
    if (pollfd->rtnevents & APR_POLLOUT)
        update_send_state(client);
}

static void
update_recv_state(ClientState *client)
{
    bool is_eof;
    bool saw_data;

    switch (client->recv_state)
    {
        case RECV_TABLE_NAME_LEN:
            saw_data = sbuf_socket_recv(client->recv_name_buf, client->sock,
                                        sizeof(apr_int16_t), &is_eof);
            if (is_eof)
                goto saw_eof;
            if (!saw_data)
                return;

            client->tbl_name_len = ntohs(sbuf_read_int16(client->recv_name_buf));
            sbuf_reset(client->recv_name_buf);
            client->recv_state = RECV_TABLE_NAME;

        case RECV_TABLE_NAME:
            saw_data = sbuf_socket_recv(client->recv_name_buf, client->sock,
                                        client->tbl_name_len, &is_eof);
            if (is_eof)
                goto saw_eof;
            if (!saw_data)
                return;

            sbuf_append_char(client->recv_name_buf, '\0');
            client->recv_state = RECV_TUPLE_LEN;

        case RECV_TUPLE_LEN:
            saw_data = sbuf_socket_recv(client->recv_tuple_buf, client->sock,
                                        sizeof(apr_int32_t), &is_eof);
            if (is_eof)
                goto saw_eof;
            if (!saw_data)
                return;

            client->tuple_len = ntohl(sbuf_read_int32(client->recv_tuple_buf));
            sbuf_reset(client->recv_tuple_buf);
            client->recv_state = RECV_TUPLE;

        case RECV_TUPLE:
            saw_data = sbuf_socket_recv(client->recv_tuple_buf, client->sock,
                                        client->tuple_len, &is_eof);
            if (is_eof)
                goto saw_eof;
            if (!saw_data)
                return;

            deserialize_tuple(client);
            sbuf_reset(client->recv_name_buf);
            sbuf_reset(client->recv_tuple_buf);
            client->recv_state = RECV_TABLE_NAME_LEN;
            return;

        default:
            ERROR("Unrecognized client receive state: %d",
                  (int) client->recv_state);
    }

saw_eof:
    if (client->recv_state != RECV_TABLE_NAME_LEN ||
        sbuf_data_avail(client->recv_name_buf))
        c4_log(client->c4, "Unexpected EOF from client @ %s",
               client->loc_spec_str);

    apr_pool_destroy(client->pool);
}

/*
 * Convert serialized tuple back into in-memory format, add to router, and
 * immediately compute a fixpoint.
 */
static void
deserialize_tuple(ClientState *client)
{
    char *tbl_name = client->recv_name_buf->data;
    Tuple *tuple;
    TableDef *tbl_def;

    ASSERT(client->recv_state == RECV_TUPLE);
    tbl_def = cat_get_table(client->c4->cat, tbl_name);
    tuple = tuple_from_buf(client->recv_tuple_buf, tbl_def->schema);
    router_insert_tuple(client->c4->router, tuple, tbl_def, false);
    router_do_fixpoint(client->c4->router);
    tuple_unpin(tuple, tbl_def->schema);
}

/*
 * We use one StrBuf for the headers (table name + length info), and
 * one StrBuf for the serialized tuple itself: we need to include the
 * length of the latter in the former.
 */
static void
serialize_tuple(ClientState *client)
{
    Tuple *tuple;
    TableDef *tbl_def;
    apr_size_t tbl_name_len;
    apr_size_t tuple_len;

    tuple_buf_shift(client->pending_tuples, &tuple, &tbl_def);

    tbl_name_len = strlen(tbl_def->name);
    if (tbl_name_len > APR_UINT16_MAX)
        FAIL();

    sbuf_append_int16(client->send_header_buf, htons(tbl_name_len));
    sbuf_append_data(client->send_header_buf, tbl_def->name,
                     tbl_name_len);

    tuple_to_buf(tuple, tbl_def->schema, client->send_tuple_buf);
    tuple_unpin(tuple, tbl_def->schema);

    tuple_len = client->send_tuple_buf->len;
    sbuf_append_int32(client->send_header_buf, htonl(tuple_len));
}

static void
update_send_state(ClientState *client)
{
    bool done_write;

    if (!client->connected)
    {
        client_try_connect(client);
        return;
    }

    switch (client->send_state)
    {
        case SEND_IDLE:
            serialize_tuple(client);
            client->send_state = SEND_HEADER;

        case SEND_HEADER:
            done_write = sbuf_socket_send(client->send_header_buf,
                                          client->sock);
            if (!done_write)
                return;

            client->send_state = SEND_TUPLE;

        case SEND_TUPLE:
            done_write = sbuf_socket_send(client->send_tuple_buf,
                                          client->sock);
            if (!done_write)
                return;

            sbuf_reset(client->send_header_buf);
            sbuf_reset(client->send_tuple_buf);
            client->send_state = SEND_IDLE;
            if (tuple_buf_is_empty(client->pending_tuples))
            {
                int reqevents = client->pollfd->reqevents;

                reqevents &= ~(APR_POLLOUT);
                update_client_interest(client, reqevents);
            }
            return;

        default:
            ERROR("Unrecognized client send state: %d",
                  (int) client->send_state);
    }
}

void
network_send(C4Network *net, Tuple *tuple, TableDef *tbl_def)
{
    ClientState *client;
    int reqevents;

    client = get_client_for_loc_spec(net, tuple, tbl_def);
    tuple_buf_push(client->pending_tuples, tuple, tbl_def);

    reqevents = client->pollfd->reqevents | APR_POLLOUT;
    update_client_interest(client, reqevents);
}

static ClientState *
get_client_for_loc_spec(C4Network *net, Tuple *tuple, TableDef *tbl_def)
{
    ClientState *client;
    Datum loc_spec;

    loc_spec = tuple_get_val(tuple, tbl_def->ls_colno);
    client = c4_hash_get(net->client_tbl, loc_spec.s);
    if (client == NULL)
    {
        client = connect_new_client(net, loc_spec);
        c4_hash_set(net->client_tbl, client->loc_spec.s, client);
    }

    return client;
}

static ClientState *
connect_new_client(C4Network *net, Datum loc_spec)
{
    ClientState *client;
    apr_status_t s;

    client = client_make(net);
    client->loc_spec = datum_copy(loc_spec, TYPE_STRING);
    pool_track_datum(client->pool, client->loc_spec, TYPE_STRING);
    client->loc_spec_str = string_to_text(client->loc_spec,
                                          client->pool);

    client->sock = create_send_socket(client, &client->remote_addr);
    client->pollfd = pollfd_make(client->pool, client->sock,
                                 APR_POLLOUT, client);
    s = apr_pollset_add(net->pollset, client->pollfd);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    client_try_connect(client);
    return client;
}

/*
 * Try to connect the client's socket to a remote host. Since this is
 * a non-blocking socket, the connection attempt may not complete
 * immediately; the socket is initially registered for APR_POLLOUT
 * events in the pollset, which should inform us when we can complete
 * the connection attempt.
 */
static void
client_try_connect(ClientState *client)
{
    apr_status_t s;
    apr_int16_t reqevents;

    ASSERT(!client->connected);
    ASSERT(client->send_state == SEND_IDLE);
    ASSERT(client->recv_state == RECV_TABLE_NAME_LEN);

    s = apr_socket_connect(client->sock, client->remote_addr);
    /* XXX: No portable APR test for EALREADY, it seems */
    if (APR_STATUS_IS_EINPROGRESS(s) || s == EALREADY)
        return;
    if (s != APR_SUCCESS)
    {
        c4_log(client->c4, "Failed to connect to remote host @ %s",
               client->loc_spec_str);
        FAIL_APR(s);
    }

    /*
     * Now that we're connected, we're ready to consume incoming
     * data. If there is pending outbound data, we're interested in
     * sending that too.
     */
    client->connected = true;
    reqevents = APR_POLLIN;
    if (!tuple_buf_is_empty(client->pending_tuples))
        reqevents |= APR_POLLOUT;

    update_client_interest(client, reqevents);
}

static void
update_client_interest(ClientState *client, int reqevents)
{
    apr_pollset_t *pollset = client->c4->net->pollset;
    apr_pollfd_t *pollfd = client->pollfd;
    apr_status_t s;

    if (pollfd->reqevents == reqevents)
        return;

    s = apr_pollset_remove(pollset, pollfd);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    pollfd->reqevents = reqevents;
    s = apr_pollset_add(pollset, pollfd);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

static apr_socket_t *
create_send_socket(ClientState *client, apr_sockaddr_t **remote_addr)
{
    apr_status_t s;
    apr_socket_t *sock;
    apr_sockaddr_t *addr;
    char host[APRMAXHOSTLEN];
    int port_num;

    parse_loc_spec(client->loc_spec_str, host, &port_num);

    s = apr_sockaddr_info_get(&addr, host, APR_INET, port_num, 0, client->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_socket_create(&sock, addr->family, SOCK_STREAM,
                          APR_PROTO_TCP, client->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    socket_set_non_block(sock);
    *remote_addr = addr;
    return sock;
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
