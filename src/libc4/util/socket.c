#include "c4-internal.h"
#include "util/socket.h"

void
socket_set_non_block(apr_socket_t *sock)
{
    apr_status_t s;

    s = apr_socket_opt_set(sock, APR_SO_NONBLOCK, 1);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_socket_timeout_set(sock, 0);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

apr_sockaddr_t *
socket_get_remote_addr(apr_socket_t *sock)
{
    apr_status_t s;
    apr_sockaddr_t *addr;

    s = apr_socket_addr_get(&addr, APR_REMOTE, sock);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return addr;
}

char *
socket_get_remote_loc(apr_socket_t *sock, apr_pool_t *pool)
{
    apr_sockaddr_t *addr;
    apr_status_t s;
    char *ip;

    addr = socket_get_remote_addr(sock);

    s = apr_sockaddr_ip_get(&ip, addr);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return apr_psprintf(pool, "tcp:%s:%hu", ip, addr->port);
}
