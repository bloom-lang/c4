#include <apr_atomic.h>

#include "col-internal.h"
#include "types/tuple.h"
#include "util/socket.h"

Tuple *
tuple_make(Schema *s, Datum *values)
{
    Tuple *t;

    t = ol_alloc(sizeof(*t) + (s->len * sizeof(Datum)));
    memset(t, 0, sizeof(*t));
    t->refcount = 1;
    /* XXX: pass-by-ref types? */
    memcpy(t + 1, values, s->len * sizeof(Datum));
    return t;
}

Tuple *
tuple_make_from_strings(Schema *s, char **values)
{
    ;
}

void
tuple_pin(Tuple *tuple)
{
    apr_uint32_t old_count = apr_atomic_inc32(&tuple->refcount);
    ASSERT(old_count > 0);
}

void
tuple_unpin(Tuple *tuple)
{
    if (apr_atomic_dec32(&tuple->refcount) == 0)
        ol_free(tuple);
}

bool
tuple_equal(Tuple *t1, Tuple *t2)
{
    Schema *s;
    int i;

    /* XXX: Should we support this case? */
    ASSERT(schema_equal(t1->schema, t2->schema));
    s = t1->schema;

    for (i = 0; i < s->len; i++)
    {
        DataType dt = schema_get_type(s, i);

        if (!datum_equal(tuple_get_val(t1, i),
                         tuple_get_val(t2, i),
                         dt))
            return false;
    }

    return true;
}

apr_uint32_t
tuple_hash(Tuple *tuple)
{
    apr_uint32_t result;
    Schema *s;
    int i;

    s = tuple->schema;
    result = 37;
    for (i = 0; i < s->len; i++)
    {
        DataType type = schema_get_type(s, i);
        Datum val = tuple_get_val(tuple, i);
        apr_uint32_t h = datum_hash(val, type);

        /* XXX: choose a better mixing function than XOR */
        result ^= h;
    }

    return result;
}

void
tuple_socket_send(Tuple *tuple, apr_socket_t *sock)
{
    socket_send_str(sock, "hello world");
}

Tuple *
tuple_from_buf(const char *buf, apr_size_t len)
{
    /* NB: Result should be pinned */
    return NULL;
}
