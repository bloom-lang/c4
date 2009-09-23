#include <apr_atomic.h>
#include <apr_hash.h>

#include "col-internal.h"
#include "types/datum.h"

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
int2_equal(Datum d1, Datum d2)
{
    return d1.i2 == d2.i2;
}

bool
int4_equal(Datum d1, Datum d2)
{
    return d1.i4 == d2.i4;
}

bool
int8_equal(Datum d1, Datum d2)
{
    return d1.i8 == d2.i8;
}

bool
string_equal(Datum d1, Datum d2)
{
    ColString *s1 = d1.s;
    ColString *s2 = d2.s;

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

        case TYPE_INT2:
            return int2_equal(d1, d2);

        case TYPE_INT4:
            return int4_equal(d1, d2);

        case TYPE_INT8:
            return int8_equal(d1, d2);

        case TYPE_STRING:
            return string_equal(d1, d2);

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

static int
bool_cmp(bool b1, bool b2)
{
    ERROR("%s: Not implemented yet", __func__);
}

static int
char_cmp(unsigned char c1, unsigned char c2)
{
    if (c1 < c2)
        return -1;
    else if (c1 > c2)
        return 1;
    else
        return 0;
}

static int
double_cmp(double d1, double d2)
{
    if (d1 < d2)
        return -1;
    else if (d1 > d2)
        return 1;
    else
        return 0;
}

static int
int2_cmp(apr_int16_t i1, apr_int16_t i2)
{
    if (i1 < i2)
        return -1;
    else if (i1 > i2)
        return 1;
    else
        return 0;
}

static int
int4_cmp(apr_int32_t i1, apr_int32_t i2)
{
    if (i1 < i2)
        return -1;
    else if (i1 > i2)
        return 1;
    else
        return 0;
}

static int
int8_cmp(apr_int64_t i1, apr_int64_t i2)
{
    if (i1 < i2)
        return -1;
    else if (i1 > i2)
        return 1;
    else
        return 0;
}

static int
string_cmp(ColString *s1, ColString *s2)
{
    ERROR("%s: Not implemented yet", __func__);
}

/* XXX: get rid of this */
int
datum_cmp(Datum d1, Datum d2, DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_cmp(d1.b, d2.b);

        case TYPE_CHAR:
            return char_cmp(d1.c, d2.c);

        case TYPE_DOUBLE:
            return double_cmp(d1.d8, d2.d8);

        case TYPE_INT2:
            return int2_cmp(d1.i2, d2.i2);

        case TYPE_INT4:
            return int4_cmp(d1.i4, d2.i4);

        case TYPE_INT8:
            return int8_cmp(d1.i8, d2.i8);

        case TYPE_STRING:
            return string_cmp(d1.s, d2.s);

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

apr_uint32_t
bool_hash(Datum d)
{
    apr_ssize_t len = sizeof(bool);
    return apr_hashfunc_default((char *) &(d.b), &len);
}

apr_uint32_t
char_hash(Datum d)
{
    apr_ssize_t len = sizeof(unsigned char);
    return apr_hashfunc_default((char *) &(d.c), &len);
}

apr_uint32_t
double_hash(Datum d)
{
    apr_ssize_t len = sizeof(double);

    /*
     * Per IEEE754, minus zero and zero may have different bit patterns, but
     * they should compare as equal. Therefore, ensure they hash to the same
     * value.
     */
    if (d.d8 == 0)
        return 0;

    return apr_hashfunc_default((char *) &(d.d8), &len);
}

apr_uint32_t
int2_hash(Datum d)
{
    apr_ssize_t len = sizeof(apr_int16_t);
    return apr_hashfunc_default((char *) &(d.i2), &len);
}

apr_uint32_t
int4_hash(Datum d)
{
    apr_ssize_t len = sizeof(apr_int32_t);
    return apr_hashfunc_default((char *) &(d.i4), &len);
}

apr_uint32_t
int8_hash(Datum d)
{
    apr_ssize_t len = sizeof(apr_int64_t);
    return apr_hashfunc_default((char *) &(d.i8), &len);
}

apr_uint32_t
string_hash(Datum d)
{
    apr_ssize_t len = d.s->len;
    return apr_hashfunc_default(d.s->data, &len);
}

/* XXX: get rid of this */
apr_uint32_t
datum_hash(Datum d, DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_hash(d);

        case TYPE_CHAR:
            return char_hash(d);

        case TYPE_DOUBLE:
            return double_hash(d);

        case TYPE_INT2:
            return int2_hash(d);

        case TYPE_INT4:
            return int4_hash(d);

        case TYPE_INT8:
            return int8_hash(d);

        case TYPE_STRING:
            return string_hash(d);

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

static void
string_pin(ColString *s)
{
    apr_atomic_inc32(&s->refcount);
}

static void
string_unpin(ColString *s)
{
    if (apr_atomic_dec32(&s->refcount) == 0)
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
    ColString *str = (ColString *) data;

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
int2_from_str(const char *str)
{
    Datum result;
    apr_int64_t raw_result;

    raw_result = parse_int64(str, APR_INT16_MAX, APR_INT16_MIN);
    result.i2 = (apr_int16_t) raw_result;
    return result;
}

Datum
int4_from_str(const char *str)
{
    Datum result;
    apr_int64_t raw_result;

    raw_result = parse_int64(str, APR_INT32_MAX, APR_INT32_MIN);
    result.i4 = (apr_int32_t) raw_result;
    return result;
}

Datum
int8_from_str(const char *str)
{
    Datum result;

    result.i8 = parse_int64(str, APR_INT64_MAX, APR_INT64_MIN);
    return result;
}

static ColString *
make_string(apr_size_t slen)
{
    ColString *s;

    s = ol_alloc(offsetof(ColString, data) + (slen * sizeof(char)));
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
int2_to_str(Datum d, StrBuf *buf)
{
    sbuf_appendf(buf, "%hd", d.i2);
}

void
int4_to_str(Datum d, StrBuf *buf)
{
    sbuf_appendf(buf, "%d", d.i4);
}

void
int8_to_str(Datum d, StrBuf *buf)
{
    sbuf_appendf(buf, "%" APR_INT64_T_FMT, d.i8);
}

void
string_to_str(Datum d, StrBuf *buf)
{
    ColString *s = d.s;
    sbuf_append_data(buf, s->data, s->len);
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
    result = int8_from_buf(buf);
    return result;
}

Datum
int2_from_buf(StrBuf *buf)
{
    Datum result;

    result.i2 = ntohs(sbuf_read_int16(buf));
    return result;
}

Datum
int4_from_buf(StrBuf *buf)
{
    Datum result;

    result.i4 = ntohl(sbuf_read_int32(buf));
    return result;
}

Datum
int8_from_buf(StrBuf *buf)
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
 * We assume that double should be byte-swapped in the same way as int8, per
 * Postgres. Apparently not perfect, but fairly portable.
 */
void
double_to_buf(Datum d, StrBuf *buf)
{
    int8_to_buf(d, buf);
}

void
int2_to_buf(Datum d, StrBuf *buf)
{
    apr_uint16_t n16;

    n16 = htons((apr_uint16_t) d.i2);
    sbuf_append_data(buf, (char *) &n16, sizeof(n16));
}

void
int4_to_buf(Datum d, StrBuf *buf)
{
    apr_uint32_t n32;

    n32 = htonl((apr_uint32_t) d.i4);
    sbuf_append_data(buf, (char *) &n32, sizeof(n32));
}

void
int8_to_buf(Datum d, StrBuf *buf)
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
    ColString *s = d.s;
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
