#include <apr_atomic.h>
#include <apr_hash.h>

#include "col-internal.h"
#include "types/datum.h"

static Datum int8_from_buf(StrBuf *buf);
static void int8_to_buf(apr_int64_t i, StrBuf *buf);

static bool
bool_equal(bool b1, bool b2)
{
    return b1 == b2;
}

static bool
char_equal(unsigned char c1, unsigned char c2)
{
    return c1 == c2;
}

static bool
double_equal(double d1, double d2)
{
    return d1 == d2;
}

static bool
int2_equal(apr_int16_t i1, apr_int16_t i2)
{
    return i1 == i2;
}

static bool
int4_equal(apr_int32_t i1, apr_int32_t i2)
{
    return i1 == i2;
}

static bool
int8_equal(apr_int64_t i1, apr_int64_t i2)
{
    return i1 == i2;
}

static bool
string_equal(ColString *s1, ColString *s2)
{
    if (s1->len != s2->len)
        return false;

    return (memcmp(s1->data, s2->data, s1->len) == 0);
}

bool
datum_equal(Datum d1, Datum d2, DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_equal(d1.b, d2.b);

        case TYPE_CHAR:
            return char_equal(d1.c, d2.c);

        case TYPE_DOUBLE:
            return double_equal(d1.d8, d2.d8);

        case TYPE_INT2:
            return int2_equal(d1.i2, d2.i2);

        case TYPE_INT4:
            return int4_equal(d1.i4, d2.i4);

        case TYPE_INT8:
            return int8_equal(d1.i8, d2.i8);

        case TYPE_STRING:
            return string_equal(d1.s, d2.s);

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");
            return false;       /* Keep compiler quiet */

        default:
            ERROR("Unexpected data type: %uc", type);
            return false;       /* Keep compiler quiet */
    }
}

static apr_uint32_t
bool_hash(bool b)
{
    apr_ssize_t len = sizeof(bool);
    return apr_hashfunc_default((char *) &b, &len);
}

static apr_uint32_t
char_hash(unsigned char c)
{
    apr_ssize_t len = sizeof(unsigned char);
    return apr_hashfunc_default((char *) &c, &len);
}

static apr_uint32_t
double_hash(double d)
{
    apr_ssize_t len = sizeof(double);

    /*
     * Per IEEE754, minus zero and zero may have different bit patterns, but
     * they should compare as equal. Therefore, ensure they hash to the same
     * value.
     */
    if (d == 0)
        return 0;

    return apr_hashfunc_default((char *) &d, &len);
}

static apr_uint32_t
int2_hash(apr_int16_t i)
{
    apr_ssize_t len = sizeof(apr_int16_t);
    return apr_hashfunc_default((char *) &i, &len);
}

static apr_uint32_t
int4_hash(apr_int32_t i)
{
    apr_ssize_t len = sizeof(apr_int32_t);
    return apr_hashfunc_default((char *) &i, &len);
}

static apr_uint32_t
int8_hash(apr_int64_t i)
{
    apr_ssize_t len = sizeof(apr_int64_t);
    return apr_hashfunc_default((char *) &i, &len);
}

static apr_uint32_t
string_hash(ColString *s)
{
    apr_ssize_t len = s->len;
    return apr_hashfunc_default(s->data, &len);
}

apr_uint32_t
datum_hash(Datum d, DataType type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_hash(d.b);

        case TYPE_CHAR:
            return char_hash(d.c);

        case TYPE_DOUBLE:
            return double_hash(d.d8);

        case TYPE_INT2:
            return int2_hash(d.i2);

        case TYPE_INT4:
            return int4_hash(d.i4);

        case TYPE_INT8:
            return int8_hash(d.i8);

        case TYPE_STRING:
            return string_hash(d.s);

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");
            return 0;       /* Keep compiler quiet */

        default:
            ERROR("Unexpected data type: %uc", type);
            return 0;       /* Keep compiler quiet */
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

static Datum
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

static Datum
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

static Datum
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

static Datum
int2_from_str(const char *str)
{
    Datum result;
    apr_int64_t raw_result;

    raw_result = parse_int64(str, APR_INT16_MAX, APR_INT16_MIN);
    result.i2 = (apr_int16_t) raw_result;
    return result;
}

static Datum
int4_from_str(const char *str)
{
    Datum result;
    apr_int64_t raw_result;

    raw_result = parse_int64(str, APR_INT32_MAX, APR_INT32_MIN);
    result.i4 = (apr_int32_t) raw_result;
    return result;
}

static Datum
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
static Datum
string_from_str(const char *str)
{
    apr_size_t slen;
    Datum result;

    slen = strlen(str);
    result.s = make_string(slen);
    memcpy(result.s->data, str, slen);
    return result;
}

Datum
datum_from_str(DataType type, const char *str)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_from_str(str);

        case TYPE_CHAR:
            return char_from_str(str);

        case TYPE_DOUBLE:
            return double_from_str(str);

        case TYPE_INT2:
            return int2_from_str(str);

        case TYPE_INT4:
            return int4_from_str(str);

        case TYPE_INT8:
            return int8_from_str(str);

        case TYPE_STRING:
            return string_from_str(str);

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

static void
bool_to_str(bool b, StrBuf *buf)
{
    sbuf_appendf(buf, "%s", (b == true) ? "true" : "false");
}

static void
char_to_str(unsigned char c, StrBuf *buf)
{
    sbuf_append_char(buf, c);
}

static void
double_to_str(double d, StrBuf *buf)
{
    sbuf_appendf(buf, "%f", d);
}

static void
int2_to_str(apr_int16_t i, StrBuf *buf)
{
    sbuf_appendf(buf, "%hd", i);
}

static void
int4_to_str(apr_int32_t i, StrBuf *buf)
{
    sbuf_appendf(buf, "%d", i);
}

static void
int8_to_str(apr_int64_t i, StrBuf *buf)
{
    sbuf_appendf(buf, "%" APR_INT64_T_FMT, i);
}

static void
string_to_str(ColString *s, StrBuf *buf)
{
    sbuf_append_data(buf, s->data, s->len);
}

void
datum_to_str(Datum d, DataType type, StrBuf *buf)
{
    switch (type)
    {
        case TYPE_BOOL:
            bool_to_str(d.b, buf);
            break;

        case TYPE_CHAR:
            char_to_str(d.c, buf);
            break;

        case TYPE_DOUBLE:
            double_to_str(d.d8, buf);
            break;

        case TYPE_INT2:
            int2_to_str(d.i2, buf);
            break;

        case TYPE_INT4:
            int4_to_str(d.i4, buf);
            break;

        case TYPE_INT8:
            int8_to_str(d.i8, buf);
            break;

        case TYPE_STRING:
            string_to_str(d.s, buf);
            break;

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

static Datum
bool_from_buf(StrBuf *buf)
{
    Datum result;

    result.b = (bool) sbuf_read_char(buf);
    ASSERT(result.b == true || result.b == false);
    return result;
}

static Datum
char_from_buf(StrBuf *buf)
{
    Datum result;

    result.c = sbuf_read_char(buf);
    return result;
}

static Datum
double_from_buf(StrBuf *buf)
{
    Datum result;

    /* See double_to_buf() for notes on this */
    result = int8_from_buf(buf);
    return result;
}

static Datum
int2_from_buf(StrBuf *buf)
{
    Datum result;

    result.i2 = ntohs(sbuf_read_int16(buf));
    return result;
}

static Datum
int4_from_buf(StrBuf *buf)
{
    Datum result;

    result.i4 = ntohl(sbuf_read_int32(buf));
    return result;
}

static Datum
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
static Datum
string_from_buf(StrBuf *buf)
{
    Datum result;
    apr_uint32_t slen;

    slen = ntohl(sbuf_read_int32(buf));
    result.s = make_string(slen);
    sbuf_read_data(buf, result.s->data, slen);
    return result;
}

/*
 * Convert a datum from the binary (network) format to the in-memory
 * format. The buffer has length "len", and we should begin reading from it
 * at "*pos"; on return, this function updates "*pos" to reflect the number
 * of bytes in the buffer that have been consumed.
 */
Datum
datum_from_buf(DataType type, StrBuf *buf)
{
    switch (type)
    {
        case TYPE_BOOL:
            return bool_from_buf(buf);

        case TYPE_CHAR:
            return char_from_buf(buf);

        case TYPE_DOUBLE:
            return double_from_buf(buf);

        case TYPE_INT2:
            return int2_from_buf(buf);

        case TYPE_INT4:
            return int4_from_buf(buf);

        case TYPE_INT8:
            return int8_from_buf(buf);

        case TYPE_STRING:
            return string_from_buf(buf);

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}

static void
bool_to_buf(bool b, StrBuf *buf)
{
    sbuf_append_data(buf, (char *) &b, sizeof(b));
}

static void
char_to_buf(unsigned char c, StrBuf *buf)
{
    sbuf_append_data(buf, (char *) &c, sizeof(c));
}

/*
 * We assume that double should be byte-swapped in the same way as int8, per
 * Postgres. Apparently not perfect, but fairly portable.
 */
static void
double_to_buf(double d, StrBuf *buf)
{
    union
    {
        double d;
        apr_int64_t i;
    } swap;

    swap.d = d;
    int8_to_buf(swap.i, buf);
}

static void
int2_to_buf(apr_int16_t i, StrBuf *buf)
{
    apr_uint16_t n16;

    n16 = htons((apr_uint16_t) i);
    sbuf_append_data(buf, (char *) &n16, sizeof(n16));
}

static void
int4_to_buf(apr_int32_t i, StrBuf *buf)
{
    apr_uint32_t n32;

    n32 = htonl((apr_uint32_t) i);
    sbuf_append_data(buf, (char *) &n32, sizeof(n32));
}

static void
int8_to_buf(apr_int64_t i, StrBuf *buf)
{
    apr_uint32_t n32;

    /* Send high-order half first */
	n32 = (apr_uint32_t) (i >> 32);
    n32 = htonl(n32);
    sbuf_append_data(buf, (char *) &n32, sizeof(n32));

    n32 = (apr_uint32_t) i;
    n32 = htonl(n32);
    sbuf_append_data(buf, (char *) &n32, sizeof(n32));
}

static void
string_to_buf(ColString *s, StrBuf *buf)
{
    apr_uint32_t net_len;

    net_len = htonl(s->len);
    sbuf_append_data(buf, (char *) &net_len, sizeof(net_len));
    sbuf_append_data(buf, s->data, s->len);
}

/*
 * Convert a datum from the in-memory format into the binary (network)
 * format. The resulting data is written to the current position in the
 * specified buffer.
 */
void
datum_to_buf(Datum d, DataType type, StrBuf *buf)
{
    switch (type)
    {
        case TYPE_BOOL:
            bool_to_buf(d.b, buf);
            break;

        case TYPE_CHAR:
            char_to_buf(d.c, buf);
            break;

        case TYPE_DOUBLE:
            double_to_buf(d.d8, buf);
            break;

        case TYPE_INT2:
            int2_to_buf(d.i2, buf);
            break;

        case TYPE_INT4:
            int4_to_buf(d.i4, buf);
            break;

        case TYPE_INT8:
            int8_to_buf(d.i8, buf);
            break;

        case TYPE_STRING:
            string_to_buf(d.s, buf);
            break;

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");

        default:
            ERROR("Unexpected data type: %uc", type);
    }
}
