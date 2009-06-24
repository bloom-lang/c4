#ifndef TUPLE_H
#define TUPLE_H

#include <apr_network_io.h>

#include "types/schema.h"

typedef union Datum
{
    apr_int32_t  i4;
    apr_int64_t  i8;
    double       d8;
    char        *s;
    bool         b;
} Datum;

typedef struct
{
    /*
     * XXX: This could be smaller, but APR's atomic ops are only defined for
     * apr_uint32_t. Do we need atomic ops in the first place?
     */
    apr_uint32_t refcount;
    Schema *schema;
    Datum *values;
} Tuple;

Tuple *tuple_make(void);
void tuple_pin(Tuple *tuple);
void tuple_unpin(Tuple *tuple);

Tuple *tuple_from_buf(const char *buf, apr_size_t len);
void tuple_socket_send(Tuple *tuple, apr_socket_t *sock);

#endif  /* TUPLE_H */
