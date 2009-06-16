#include <arpa/inet.h>
#include <stdlib.h>

#include "col-internal.h"
#include "types/tuple.h"

Tuple *
tuple_make()
{
    Tuple *t = (Tuple *) malloc(sizeof(Tuple));
    return t;
}

static void
sock_send_str(apr_socket_t *sock, const char *str)
{
    apr_status_t s;
    apr_size_t slen;
    uint32_t slen_net;
    apr_size_t len_len;

    slen = strlen(str);
    slen_net = htonl(slen);
    len_len = sizeof(slen_net);

    s = apr_socket_send(sock, (char *) &slen_net, &len_len);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_socket_send(sock, str, &slen);
    if (s != APR_SUCCESS)
        FAIL();
}

void
tuple_socket_send(Tuple *tuple, apr_socket_t *sock)
{
    sock_send_str(sock, "hello world");
}

Tuple *
tuple_from_buf(const char *buf, apr_size_t len)
{
    return NULL;
}
