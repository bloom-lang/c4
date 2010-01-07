#include "c4-internal.h"
#include "util/socket.h"

apr_uint32_t
socket_recv_uint32(apr_socket_t *sock, bool *is_eof)
{
    apr_status_t s;
    apr_size_t len;
    char buf[4];
    apr_uint32_t raw_result;

    len = sizeof(buf);
    s = apr_socket_recv(sock, buf, &len);
    if (s == APR_EOF)
    {
        *is_eof = true;
        /*
         * XXX: We silently drop any incomplete data we might have read (APR
         * allows len > 0 even in the case of failure).
         */
        return 0;
    }
    if (s != APR_SUCCESS)
        FAIL_APR(s);
    if (len != sizeof(buf))
        FAIL();

    *is_eof = false;
    memcpy(&raw_result, buf, sizeof(buf));
    return ntohl(raw_result);
}

void
socket_recv_data(apr_socket_t *sock, char *buf, apr_size_t buf_len, bool *is_eof)
{
    apr_status_t s;
    apr_size_t len;

    len = buf_len;
    s = apr_socket_recv(sock, buf, &len);
    if (s == APR_EOF)
    {
        *is_eof = true;
        return;
    }
    if (s != APR_SUCCESS)
        FAIL_APR(s);
    if (len != buf_len)
        FAIL();

    *is_eof = false;
}

void
socket_send_uint32(apr_socket_t *sock, apr_uint32_t u32)
{
    apr_status_t s;
    apr_uint32_t u32_net;
    apr_size_t len;

    u32_net = htonl(u32);
    len = sizeof(u32_net);

    s = apr_socket_send(sock, (char *) &u32_net, &len);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
    if (len != sizeof(u32_net))
        FAIL();
}

void
socket_send_data(apr_socket_t *sock, const char *buf, apr_size_t len)
{
    apr_status_t s;
    apr_size_t orig_len;

    orig_len = len;
    s = apr_socket_send(sock, buf, &len);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
    if (len != orig_len)
        FAIL();
}

/*
 * Write the given string to the socket, preceded by a length word. Note
 * that we assume that the input string is NUL-terminated, but we don't
 * write the NUL-terminator to the socket.
 */
void
socket_send_str(apr_socket_t *sock, const char *str)
{
    apr_size_t slen;

    slen = strlen(str);
    socket_send_uint32(sock, slen);
    socket_send_data(sock, str, slen);
}

void
socket_set_non_block(apr_socket_t *sock)
{
    apr_socket_t s;

    s = apr_socket_opt_set(sock, APR_SO_NONBLOCK, 1);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_socket_timeout_set(sock, 0);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

char *
socket_get_remote_loc(apr_socket_t *sock, apr_pool_t *pool)
{
    apr_status_t s;
    apr_sockaddr_t *addr;
    char *ip;

    s = apr_socket_addr_get(&addr, APR_REMOTE, sock);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_sockaddr_ip_get(&ip, addr);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return apr_psprintf(pool, "tcp:%s:%hu", ip, addr->port);
}
