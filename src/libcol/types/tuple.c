#include <apr_atomic.h>

#include "col-internal.h"
#include "types/catalog.h"
#include "types/tuple.h"
#include "util/socket.h"

Tuple *
tuple_make_empty(Schema *s)
{
    Tuple *t;

    t = ol_alloc(offsetof(Tuple, vals) + (s->len * sizeof(Datum)));
    t->schema = s;
    t->refcount = 1;

    return t;
}

Tuple *
tuple_make(Schema *s, Datum *values)
{
    Tuple *t;

    t = tuple_make_empty(s);
    /* XXX: pass-by-ref types? */
    memcpy(t->vals, values, s->len * sizeof(Datum));
    return t;
}

Tuple *
tuple_make_from_strings(Schema *s, char **values)
{
    Tuple *t;
    int i;

    t = tuple_make_empty(s);

    for (i = 0; i < s->len; i++)
    {
        DataType type;

        type = schema_get_type(s, i);
        tuple_get_val(t, i) = datum_from_str(type, values[i]);
    }

    return t;
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
    {
        Schema *s = tuple->schema;
        int i;

        for (i = 0; i < s->len; i++)
            datum_free(tuple_get_val(tuple, i),
                       schema_get_type(s, i));
    }
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

/*
 * XXX: Note that we return a malloc'd string, with a cleanup function
 * registered in the given context. This might get expensive if used
 * frequently...
 */
char *
tuple_to_str(Tuple *tuple, apr_pool_t *pool)
{
    Schema *schema = tuple->schema;
    StrBuf *buf;
    int i;

    buf = sbuf_make(pool);
    for (i = 0; i < schema->len; i++)
    {
        DataType type;

        if (i != 0)
            sbuf_append_char(buf, ',');

        type = schema_get_type(schema, i);
        datum_to_str(tuple_get_val(tuple, i), type, buf);
    }

    sbuf_append_char(buf, '\0');
    return buf->data;
}

void
tuple_to_buf(Tuple *tuple, StrBuf *buf)
{
    Schema *schema = tuple->schema;
    int i;

    for (i = 0; i < schema->len; i++)
    {
        DataType type = schema_get_type(schema, i);
        datum_to_buf(tuple_get_val(tuple, i), type, buf);
    }
}

Tuple *
tuple_from_buf(ColInstance *col, StrBuf *buf, const char *tbl_name)
{
    Schema *schema;
    Tuple *result;
    apr_size_t buf_pos;
    int i;

    schema = cat_get_schema(col->cat, tbl_name);
    result = tuple_make_empty(schema);
    buf_pos = 0;

    for (i = 0; i < schema->len; i++)
    {
        DataType type;

        type = schema_get_type(schema, i);
        tuple_get_val(result, i) = datum_from_buf(type, buf);
    }

    return result;
}

bool
tuple_is_remote(Tuple *tuple, TableDef *tbl_def, ColInstance *col)
{
    Datum tuple_addr;

    if (tbl_def->ls_colno == -1)
        return false;

    tuple_addr = tuple_get_val(tuple, tbl_def->ls_colno);
    return (datum_equal(col->local_addr, tuple_addr, TYPE_STRING) == false);
}
