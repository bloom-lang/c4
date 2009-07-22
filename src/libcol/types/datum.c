#include <apr_hash.h>

#include "col-internal.h"
#include "types/datum.h"

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

static Datum
string_from_str(const char *str)
{
    Datum result;

    result.b = true;
    return result;
}

Datum
datum_from_str(const char *str, DataType type)
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
