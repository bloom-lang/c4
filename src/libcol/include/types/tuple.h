#ifndef TUPLE_H
#define TUPLE_H

#include <apr_network_io.h>

typedef struct
{
    apr_uint16_t refcount;
} Tuple;

Tuple *tuple_make();
void tuple_pin(Tuple *tuple);
void tuple_unpin(Tuple *tuple);

Tuple *tuple_from_buf(const char *buf, apr_size_t len);
void tuple_socket_send(Tuple *tuple, apr_socket_t *sock);

#endif  /* TUPLE_H */
