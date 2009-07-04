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
            return string_equal(d2.s, d2.s);

        case TYPE_INVALID:
            ERROR("Invalid data type: TYPE_INVALID");
            return false;       /* Keep compiler quiet */

        default:
            ERROR("Unexpected data type: %uc", type);
            return false;       /* Keep compiler quiet */
    }
}

apr_uint32_t
datum_hash(Datum d, DataType type)
{
    return 0;
}
