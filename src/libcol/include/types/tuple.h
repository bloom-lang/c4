#ifndef TUPLE_H
#define TUPLE_H

#include <apr_network_io.h>

typedef struct
{
    int foo;
} Tuple;

Tuple *tuple_make();

Tuple *tuple_from_buf(const char *buf, apr_size_t len);
void tuple_socket_send(Tuple *tuple, apr_socket_t *sock);

#endif  /* TUPLE_H */
