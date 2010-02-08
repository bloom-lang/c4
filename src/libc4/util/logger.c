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

void
c4_warn_apr(C4Runtime *c4, apr_status_t s, const char *fmt, ...)
{
    va_list args;
    char *fmt_str;
    char apr_buf[512];

    va_start(args, fmt);
    fmt_str = apr_pvsprintf(c4->log->tmp_pool, fmt, args);
    va_end(args);

    apr_strerror(s, apr_buf, sizeof(apr_buf));

    fprintf(stdout, "WARN (%d): %s: \"%s\"\n", c4->port, fmt_str, apr_buf);
    apr_pool_clear(c4->log->tmp_pool);
}

char *
log_tuple(C4Runtime *c4, Tuple *tuple, Schema *s)
{
    char *tuple_str = tuple_to_str(tuple, s, c4->log->tmp_pool);
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
