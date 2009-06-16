#include <stdlib.h>

#include "col-internal.h"
#include "types/tuple.h"
#include "util/socket.h"

Tuple *
tuple_make()
{
    Tuple *t = (Tuple *) malloc(sizeof(Tuple));
    t->refcount = 1;
    return t;
}

void
tuple_pin(Tuple *tuple)
{
    ;
}

void
tuple_unpin(Tuple *tuple)
{
    ;
}

void
tuple_socket_send(Tuple *tuple, apr_socket_t *sock)
{
    socket_send_str(sock, "hello world");
}

Tuple *
tuple_from_buf(const char *buf, apr_size_t len)
{
    return NULL;
}
