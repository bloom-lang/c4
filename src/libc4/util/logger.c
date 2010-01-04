#include <stdarg.h>

#include "c4-internal.h"

struct C4Logger
{
    C4Runtime *c4;
    /* This is reset on each call to c4_log() */
    apr_pool_t *tmp_pool;
};

C4Logger *
logger_make(C4Runtime *c4)
{
    C4Logger *logger;

    logger = apr_pcalloc(c4->pool, sizeof(*logger));
    logger->tmp_pool = make_subpool(c4->pool);

    return logger;
}

void
c4_log(C4Runtime *c4, const char *fmt, ...)
{
    va_list args;
    char *str;

    va_start(args, fmt);
    str = apr_pvsprintf(c4->log->tmp_pool, fmt, args);
    va_end(args);

    fprintf(stdout, "LOG (%d): %s\n", c4->port, str);
    apr_pool_clear(c4->log->tmp_pool);
}

char *
log_tuple(C4Runtime *c4, Tuple *tuple)
{
    char *tuple_str = tuple_to_str(tuple, c4->log->tmp_pool);
    return apr_pstrcat(c4->log->tmp_pool, "{", tuple_str, "}", NULL);
}

char *
log_datum(C4Runtime *c4, Datum datum, DataType type)
{
    StrBuf *sbuf;

    sbuf = sbuf_make(c4->log->tmp_pool);
    datum_to_str(datum, type, sbuf);
    sbuf_append_char(sbuf, '\0');
    return sbuf->data;
}
