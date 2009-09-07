#ifndef DATUM_H
#define DATUM_H

#include "types/schema.h"
#include "util/strbuf.h"

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
