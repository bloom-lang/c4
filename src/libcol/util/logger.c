#include <stdarg.h>

#include "col-internal.h"

struct ColLogger
{
    ColInstance *col;
    /* This is reset on each call to col_log() */
    apr_pool_t *tmp_pool;
};

ColLogger *
logger_make(ColInstance *col)
{
    ColLogger *logger;

    logger = apr_pcalloc(col->pool, sizeof(*logger));
    logger->tmp_pool = make_subpool(col->pool);

    return logger;
}

void
col_log(ColInstance *col, const char *fmt, ...)
{
    va_list args;
    char *str;

    va_start(args, fmt);
    str = apr_pvsprintf(col->log->tmp_pool, fmt, args);
    va_end(args);

    fprintf(stdout, "LOG: %s\n", str);
    apr_pool_clear(col->log->tmp_pool);
}

char *
log_tuple(ColInstance *col, Tuple *tuple)
{
    char *tuple_str = tuple_to_str(tuple, col->log->tmp_pool);
    return apr_pstrcat(col->log->tmp_pool, "{", tuple_str, "}", NULL);
}

char *
log_datum(ColInstance *col, Datum datum, DataType type)
{
    StrBuf *sbuf;

    sbuf = sbuf_make(col->log->tmp_pool);
    datum_to_str(datum, type, sbuf);
    sbuf_append_char(sbuf, '\0');
    return sbuf->data;
}
