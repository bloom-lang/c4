#include <apr_atomic.h>

#include "c4-internal.h"
#include "types/catalog.h"
#include "types/tuple.h"
#include "util/socket.h"

Tuple *
tuple_make_empty(Schema *s)
{
    Tuple *t;

    t = tuple_pool_loan(s->tuple_pool);
    ASSERT(tuple_get_schema(t) == s);

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
        tuple_get_val(t, i) = (s->text_in_funcs[i])(values[i]);
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
        Schema *s = tuple_get_schema(tuple);
        int i;

        for (i = 0; i < s->len; i++)
            datum_free(tuple_get_val(tuple, i),
                       schema_get_type(s, i));

        tuple_pool_return(s->tuple_pool, tuple);
    }
}

bool
tuple_equal(Tuple *t1, Tuple *t2)
{
    Schema *s;
    int i;

    s = tuple_get_schema(t1);
    /* XXX: Should we support this case? */
    ASSERT(schema_equal(s, tuple_get_schema(t2)));

    for (i = 0; i < s->len; i++)
    {
        Datum val1 = tuple_get_val(t1, i);
        Datum val2 = tuple_get_val(t2, i);

        if ((s->eq_funcs[i])(val1, val2) == false)
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

    s = tuple_get_schema(tuple);
    result = 37;
    for (i = 0; i < s->len; i++)
    {
        Datum val = tuple_get_val(tuple, i);
        apr_uint32_t h = (s->hash_funcs[i])(val);

        /* XXX: choose a better mixing function than XOR */
        result ^= h;
    }

    return result;
}

/*
 * XXX: Note that we return a malloc'd string, with a cleanup function
 * registered in the given pool. This might get expensive if used
 * frequently...
 */
char *
tuple_to_str(Tuple *tuple, apr_pool_t *pool)
{
    StrBuf *buf;

    buf = sbuf_make(pool);
    tuple_to_str_buf(tuple, buf);
    sbuf_append_char(buf, '\0');
    return buf->data;
}

void
tuple_to_str_buf(Tuple *tuple, StrBuf *buf)
{
    Schema *schema;
    int i;

    schema = tuple_get_schema(tuple);
    for (i = 0; i < schema->len; i++)
    {
        if (i != 0)
            sbuf_append_char(buf, ',');

        (schema->text_out_funcs[i])(tuple_get_val(tuple, i), buf);
    }

    /* Note that we don't NUL-terminate the buffer */
}

void
tuple_to_buf(Tuple *tuple, StrBuf *buf)
{
    Schema *schema;
    int i;

    schema = tuple_get_schema(tuple);
    for (i = 0; i < schema->len; i++)
    {
        (schema->bin_out_funcs[i])(tuple_get_val(tuple, i), buf);
    }
}

Tuple *
tuple_from_buf(StrBuf *buf, TableDef *tbl_def)
{
    Schema *schema;
    Tuple *result;
    int i;

    schema = tbl_def->schema;
    result = tuple_make_empty(schema);

    for (i = 0; i < schema->len; i++)
    {
        tuple_get_val(result, i) = (schema->bin_in_funcs[i])(buf);
    }

    return result;
}

/*
 * XXX: Note that we return a malloc'd string, with a cleanup function
 * registered in the given context. This might get expensive if used
 * frequently...
 */
char *
tuple_to_sql_insert_str(Tuple *tuple, apr_pool_t *pool)
{
    Schema *schema;
    StrBuf *buf;
    int i;

    schema = tuple_get_schema(tuple);
    buf = sbuf_make(pool);
    for (i = 0; i < schema->len; i++)
    {
        if (i != 0)
            sbuf_append_char(buf, ',');
        if (schema->types[i] == TYPE_STRING)
            sbuf_append_char(buf, '\'');

        (schema->text_out_funcs[i])(tuple_get_val(tuple, i), buf);

        if (schema->types[i] == TYPE_STRING)
            sbuf_append_char(buf, '\'');
    }

    sbuf_append_char(buf, '\0');
    return buf->data;
}

bool
tuple_is_remote(Tuple *tuple, TableDef *tbl_def, C4Runtime *c4)
{
    Datum tuple_addr;

    if (tbl_def->ls_colno == -1)
        return false;

    tuple_addr = tuple_get_val(tuple, tbl_def->ls_colno);
    return (string_equal(c4->local_addr, tuple_addr) == false);
}
