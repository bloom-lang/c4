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

typedef struct C4String
{
    /* The number of bytes in "data"; XXX: including NUL terminator? */
    apr_uint32_t len;
    apr_uint16_t refcount;
    char data[1];       /* Variable-sized */
} C4String;

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
    C4String     *s;
} Datum;

typedef bool (*datum_eq_func)(Datum d1, Datum d2);
typedef apr_uint32_t (*datum_hash_func)(Datum d);
typedef Datum (*datum_bin_in_func)(StrBuf *buf);
typedef Datum (*datum_text_in_func)(const char *str);
typedef void (*datum_bin_out_func)(Datum d, StrBuf *buf);
typedef void (*datum_text_out_func)(Datum d, StrBuf *buf);

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

/* Binary input functions */
Datum bool_from_buf(StrBuf *buf);
Datum char_from_buf(StrBuf *buf);
Datum double_from_buf(StrBuf *buf);
Datum int2_from_buf(StrBuf *buf);
Datum int4_from_buf(StrBuf *buf);
Datum int8_from_buf(StrBuf *buf);
Datum string_from_buf(StrBuf *buf);

/* Binary output functions */
void bool_to_buf(Datum d, StrBuf *buf);
void char_to_buf(Datum d, StrBuf *buf);
void double_to_buf(Datum d, StrBuf *buf);
void int2_to_buf(Datum d, StrBuf *buf);
void int4_to_buf(Datum d, StrBuf *buf);
void int8_to_buf(Datum d, StrBuf *buf);
void string_to_buf(Datum d, StrBuf *buf);

/* Text input functions */
Datum bool_from_str(const char *str);
Datum char_from_str(const char *str);
Datum double_from_str(const char *str);
Datum int2_from_str(const char *str);
Datum int4_from_str(const char *str);
Datum int8_from_str(const char *str);
Datum string_from_str(const char *str);

/* Text output functions */
void bool_to_str(Datum d, StrBuf *buf);
void char_to_str(Datum d, StrBuf *buf);
void double_to_str(Datum d, StrBuf *buf);
void int2_to_str(Datum d, StrBuf *buf);
void int4_to_str(Datum d, StrBuf *buf);
void int8_to_str(Datum d, StrBuf *buf);
void string_to_str(Datum d, StrBuf *buf);

char *string_to_text(Datum d, apr_pool_t *pool);

bool datum_equal(Datum d1, Datum d2, DataType type);
int datum_cmp(Datum d1, Datum d2, DataType type);
Datum datum_copy(Datum d, DataType type);
void datum_free(Datum d, DataType type);
void pool_track_datum(apr_pool_t *pool, Datum datum, DataType type);

void datum_to_str(Datum d, DataType type, StrBuf *buf);
Datum datum_from_str(DataType type, const char *str);

#endif  /* DATUM_H */
