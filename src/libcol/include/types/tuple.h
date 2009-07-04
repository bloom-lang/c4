#ifndef TUPLE_H
#define TUPLE_H

#include <apr_network_io.h>

#include "types/datum.h"
#include "types/schema.h"

/*
 * XXX: Note that tuple refcounts are not currently tracked by the APR pool
 * cleanup mechanism. Therefore, modules keeping a tuple pinned should be
 * sure to release their pin in a cleanup function.
 */
typedef struct Tuple
{
    /*
     * XXX: This could be smaller, but APR's atomic ops are only defined for
     * apr_uint32_t. (Atomic ops are used because we might be touching the
     * refcount from multiple threads concurrently.)
     */
    apr_uint32_t refcount;
    Schema *schema;
    /* XXX: Unnecessary padding on a 32-bit machine */
    char c[1];
    /* Tuple data follows, as a variable-length array of Datum */
} Tuple;

#define tuple_get_data(t)       ((Datum *) (((Tuple *) t) + 1))
#define tuple_get_val(t, i)     (tuple_get_data(t)[(i)])

Tuple *tuple_make(Schema *s, Datum *values);
void tuple_pin(Tuple *tuple);
void tuple_unpin(Tuple *tuple);

bool tuple_equal(Tuple *t1, Tuple *t2);
apr_uint32_t tuple_hash(Tuple *t);

Tuple *tuple_from_buf(const char *buf, apr_size_t len);
void tuple_socket_send(Tuple *tuple, apr_socket_t *sock);

#endif  /* TUPLE_H */
