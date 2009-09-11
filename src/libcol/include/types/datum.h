#ifndef DATUM_H
#define DATUM_H

#include "util/strbuf.h"

/*
 * This is effectively a set of type IDs. A more sophisticated type ID
 * system is probably a natural next step.
 */
typedef unsigned char DataType;

#define TYPE_INVALID 0
#define TYPE_BOOL    1
#define TYPE_CHAR    2
#define TYPE_DOUBLE  3
#define TYPE_INT2    4
#define TYPE_INT4    5
#define TYPE_INT8    6
#define TYPE_STRING  7

typedef struct ColString
{
    /* The number of bytes in "data"; XXX: including NUL terminator? */
    apr_uint32_t len;
    apr_uint32_t refcount;
    char data[1];       /* Variable-sized */
} ColString;

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
    ColString     *s;
} Datum;

typedef bool (*datum_eq_func)(Datum d1, Datum d2);
typedef apr_uint32_t (*datum_hash_func)(Datum d);

bool bool_equal(Datum d1, Datum d2);
bool char_equal(Datum d1, Datum d2);
bool double_equal(Datum d1, Datum d2);
bool int2_equal(Datum d1, Datum d2);
bool int4_equal(Datum d1, Datum d2);
bool int8_equal(Datum d1, Datum d2);
bool string_equal(Datum d1, Datum d2);

apr_uint32_t bool_hash(Datum d);
apr_uint32_t char_hash(Datum d);
apr_uint32_t double_hash(Datum d);
apr_uint32_t int2_hash(Datum d);
apr_uint32_t int4_hash(Datum d);
apr_uint32_t int8_hash(Datum d);
apr_uint32_t string_hash(Datum d);

bool datum_equal(Datum d1, Datum d2, DataType type);
int datum_cmp(Datum d1, Datum d2, DataType type);
apr_uint32_t datum_hash(Datum d1, DataType type);
Datum datum_copy(Datum d, DataType type);
void datum_free(Datum d, DataType type);
void pool_track_datum(apr_pool_t *pool, Datum datum, DataType type);

Datum datum_from_str(DataType type, const char *str);
void datum_to_str(Datum d, DataType type, StrBuf *buf);

Datum datum_from_buf(DataType type, StrBuf *buf);
void datum_to_buf(Datum d, DataType type, StrBuf *buf);

#endif  /* DATUM_H */
