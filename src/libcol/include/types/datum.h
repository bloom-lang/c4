#ifndef DATUM_H
#define DATUM_H

#include "types/schema.h"

typedef struct ColString
{
    apr_uint32_t len;
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
apr_uint32_t datum_hash(Datum d1, DataType type);

#endif  /* DATUM_H */
