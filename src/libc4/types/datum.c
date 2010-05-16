#include <apr_hash.h>

#include "c4-internal.h"
#include "types/datum.h"
#include "util/hash_func.h"

bool
bool_equal(Datum d1, Datum d2)
{
    return d1.b == d2.b;
}

bool
char_equal(Datum d1, Datum d2)
{
    return d1.c == d2.c;
}

bool
double_equal(Datum d1, Datum d2)
{
    return d1.d8 == d2.d8;
}

bool
int_equal(Datum d1, Datum d2)
{
    return d1.i8 == d2.i8;
}

bool
string_equal(Datum d1, Datum d2)
{
    C4String *s1 = d1.s;
    C4String *s2 = d2.s;

    if (s1->len != s2->len)
        return false;

    return (memcmp(s1->data, s2->data, s1->len) == 0);
}

/* XXX: get rid of this */
bool
datum_equal(Datum d1, Datum d2, DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_equal(d1, d2);

        case TYPE_CHAR:
            return char_equal(d1, d2);

        case TYPE_DOUBLE:
            return double_equal(d1, d2);

        case TYPE_INT:
            return int_equal(d1, d2);

        case TYPE_STRING:
            return string_equal(d1, d2);

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

int
bool_cmp(Datum d1, Datum d2)
{
    if (d1.b < d2.b)
        return -1;
    else if (d1.b > d2.c)
        return 1;
    else
        return 0;
}

int
char_cmp(Datum d1, Datum d2)
{
    if (d1.c < d2.c)
        return -1;
    else if (d1.c > d2.c)
        return 1;
    else
        return 0;
}

int
double_cmp(Datum d1, Datum d2)
{
    if (d1.d8 < d2.d8)
        return -1;
    else if (d1.d8 > d2.d8)
        return 1;
    else
        return 0;
}

int
int_cmp(Datum d1, Datum d2)
{
    if (d1.i8 < d2.i8)
        return -1;
    else if (d1.i8 > d2.i8)
        return 1;
    else
        return 0;
}

int
string_cmp(Datum d1, Datum d2)
{
    C4String *s1 = d1.s;
    C4String *s2 = d2.s;
    int result;

    result = memcmp(s1->data, s2->data, Min(s1->len, s2->len));
    if ((result == 0) && (s1->len != s2->len))
        result = (s1->len < s2->len ? -1 : 1);

    return result;
}

/* XXX: get rid of this */
int
datum_cmp(Datum d1, Datum d2, DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_cmp(d1, d2);

        case TYPE_CHAR:
            return char_cmp(d1, d2);

        case TYPE_DOUBLE:
            return double_cmp(d1, d2);

        case TYPE_INT:
            return int_cmp(d1, d2);

        case TYPE_STRING:
            return string_cmp(d1, d2);

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

apr_uint32_t
bool_hash(Datum d)
{
    return hash_any((unsigned char *) &(d.b), sizeof(bool));
}

apr_uint32_t
char_hash(Datum d)
{
    return hash_any((unsigned char *) &(d.c), sizeof(unsigned char));
}

apr_uint32_t
double_hash(Datum d)
{
    /*
     * Per IEEE754, minus zero and zero may have different bit patterns, but
     * they should compare as equal. Therefore, ensure they hash to the same
     * value.
     */
    if (d.d8 == 0)
        return 0;

    return hash_any((unsigned char *) &(d.d8), sizeof(double));
}

apr_uint32_t
int_hash(Datum d)
{
    return hash_any((unsigned char *) &(d.i8), sizeof(apr_int64_t));
}

apr_uint32_t
string_hash(Datum d)
{
    return hash_any((unsigned char *) d.s->data, d.s->len);
}

static void
string_pin(C4String *s)
{
    s->refcount++;
}

static void
string_unpin(C4String *s)
{
    ASSERT(s->refcount >= 1);
    s->refcount--;
    if (s->refcount == 0)
        ol_free(s);
}

/* XXX: Consider inlining this function */
Datum
datum_copy(Datum in, DataType type)
{
    if (type == TYPE_STRING)
        string_pin(in.s);

    return in;
}

/* XXX: Consider inlining this function */
void
datum_free(Datum in, DataType type)
{
    if (type == TYPE_STRING)
        string_unpin(in.s);
}

static apr_status_t
datum_cleanup(void *data)
{
    /* XXX: We assume the input datum is a string */
    C4String *str = (C4String *) data;

    string_unpin(str);
    return APR_SUCCESS;
}

/*
 * Instruct the specified pool to "track" the given Datum -- that is, when
 * the pool is cleared or destroyed, the Datum's refcount will be
 * decremented once.
 */
void
pool_track_datum(apr_pool_t *pool, Datum datum, DataType type)
{
    /* Right now, strings are the only pass-by-ref datums */
    if (type == TYPE_STRING)
        apr_pool_cleanup_register(pool, datum.s, datum_cleanup,
                                  apr_pool_cleanup_null);
}

Datum
bool_from_str(const char *str)
{
    Datum result;

    if (strcmp(str, "true") == 0)
        result.b = true;
    else if (strcmp(str, "false") == 0)
        result.b = false;
    else
        FAIL();

    return result;
}

Datum
char_from_str(const char *str)
{
    Datum result;

    if (str[0] == '\0')
        FAIL();
    if (str[1] != '\0')
        FAIL();

    result.c = (unsigned char) str[0];
    return result;
}

Datum
double_from_str(const char *str)
{
    Datum result;
    char *end_ptr;

    errno = 0;
    result.d8 = strtod(str, &end_ptr);
    if (errno != 0 || str == end_ptr)
        FAIL();
    if (*end_ptr != '\0')
        FAIL(); /* XXX: should we allow trailing whitespace? */

    return result;
}

static apr_int64_t
parse_int64(const char *str, apr_int64_t max, apr_int64_t min)
{
    apr_int64_t result;

    errno = 0;
    result = apr_atoi64(str);
    if (errno != 0)
        FAIL();
    if (result > max || result < min)
        FAIL();

    return result;
}

Datum
int_from_str(const char *str)
{
    Datum result;

    result.i8 = parse_int64(str, APR_INT64_MAX, APR_INT64_MIN);
    return result;
}

C4String *
make_string(apr_size_t slen)
{
    C4String *s;

    s = ol_alloc(offsetof(C4String, data) + (slen * sizeof(char)));
    s->len = slen;
    s->refcount = 1;
    return s;
}

/* FIXME */
Datum
string_from_str(const char *str)
{
    apr_size_t slen;
    Datum result;

    slen = strlen(str);
    result.s = make_string(slen);
    memcpy(result.s->data, str, slen);
    return result;
}

void
bool_to_str(Datum d, StrBuf *buf)
{
    sbuf_appendf(buf, "%s", (d.b == true) ? "true" : "false");
}

void
char_to_str(Datum d, StrBuf *buf)
{
    sbuf_append_char(buf, d.c);
}

void
double_to_str(Datum d, StrBuf *buf)
{
    sbuf_appendf(buf, "%f", d.d8);
}

void
int_to_str(Datum d, StrBuf *buf)
{
    sbuf_appendf(buf, "%" APR_INT64_T_FMT, d.i8);
}

void
string_to_str(Datum d, StrBuf *buf)
{
    C4String *s = d.s;
    sbuf_append_data(buf, s->data, s->len);
}

/*
 * XXX: This is asymmetric with the rest of the APIs, but it is useful to
 * convert a C4String to a C-style string without needing to use a temporary
 * StrBuf. Flesh this out into a generic API?
 */
char *
string_to_text(Datum d, apr_pool_t *pool)
{
    C4String *s = d.s;
    return apr_pstrmemdup(pool, s->data, s->len);
}

Datum
bool_from_buf(StrBuf *buf)
{
    Datum result;

    result.b = (bool) sbuf_read_char(buf);
    ASSERT(result.b == true || result.b == false);
    return result;
}

Datum
char_from_buf(StrBuf *buf)
{
    Datum result;

    result.c = sbuf_read_char(buf);
    return result;
}

Datum
double_from_buf(StrBuf *buf)
{
    Datum result;

    /* See double_to_buf() for notes on this */
    result = int_from_buf(buf);
    return result;
}

Datum
int_from_buf(StrBuf *buf)
{
    Datum result;
    apr_uint32_t h32;
    apr_uint32_t l32;

    h32 = ntohl(sbuf_read_int32(buf));
    l32 = ntohl(sbuf_read_int32(buf));

    result.i8 = h32;
    result.i8 <<= 32;
    result.i8 |= l32;
    return result;
}

/* FIXME */
Datum
string_from_buf(StrBuf *buf)
{
    Datum result;
    apr_uint32_t slen;

    slen = ntohl(sbuf_read_int32(buf));
    result.s = make_string(slen);
    sbuf_read_data(buf, result.s->data, slen);
    return result;
}

void
bool_to_buf(Datum d, StrBuf *buf)
{
    bool b = d.b;
    sbuf_append_data(buf, (char *) &b, sizeof(b));
}

void
char_to_buf(Datum d, StrBuf *buf)
{
    unsigned char c = d.c;
    sbuf_append_data(buf, (char *) &c, sizeof(c));
}

/*
 * We assume that double should be byte-swapped in the same way as int, per
 * Postgres. Apparently not perfect, but fairly portable.
 */
void
double_to_buf(Datum d, StrBuf *buf)
{
    int_to_buf(d, buf);
}

void
int_to_buf(Datum d, StrBuf *buf)
{
    apr_int64_t i = d.i8;
    apr_uint32_t n32;

    /* Send high-order half first */
    n32 = (apr_uint32_t) (i >> 32);
    n32 = htonl(n32);
    sbuf_append_data(buf, (char *) &n32, sizeof(n32));

    n32 = (apr_uint32_t) i;
    n32 = htonl(n32);
    sbuf_append_data(buf, (char *) &n32, sizeof(n32));
}

void
string_to_buf(Datum d, StrBuf *buf)
{
    C4String *s = d.s;
    apr_uint32_t net_len;

    net_len = htonl(s->len);
    sbuf_append_data(buf, (char *) &net_len, sizeof(net_len));
    sbuf_append_data(buf, s->data, s->len);
}

void
datum_to_str(Datum d, DataType type, StrBuf *buf)
{
    datum_text_out_func out_func;

    out_func = type_get_text_out_func(type);
    out_func(d, buf);
}

Datum
datum_from_str(DataType type, const char *str)
{
    datum_text_in_func in_func;

    in_func = type_get_text_in_func(type);
    return in_func(str);
}
