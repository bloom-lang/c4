#ifndef TUPLE_H
#define TUPLE_H

#include <apr_network_io.h>

#include "types/schema.h"

typedef union Datum
{
    /* Pass-by-value types (unboxed) */
    bool           b;
    unsigned char  c;
    apr_int16_t    i2;
    apr_int32_t    i4;
    apr_int64_t    i8;
    double         d8;
    /* Pass-by-ref types (boxed) */
    char          *s;
} Datum;

typedef struct Tuple
{
    /*
     * XXX: This could be smaller, but APR's atomic ops are only defined for
     * apr_uint32_t. (Atomic ops are for used because we might be touching
     * the refcount from multiple threads concurrently.)
     */
    apr_uint32_t refcount;
    Schema *schema;
    /* XXX: Unnecessary padding on a 32-bit machine */
    char c[1];
    /* Tuple data follows, as a variable-length array of Datum */
} Tuple;

#define tuple_get_data(t)       (((Tuple *) t) + 1)
#define tuple_get_val(t, i)     (tuple_get_data(t)[(i)])

Tuple *tuple_make(Schema *s, Datum *values);
void tuple_pin(Tuple *tuple);
void tuple_unpin(Tuple *tuple);

Tuple *tuple_from_buf(const char *buf, apr_size_t len);
void tuple_socket_send(Tuple *tuple, apr_socket_t *sock);

#endif  /* TUPLE_H */
