#include "col-internal.h"
#include "util/socket.h"
#include "util/strbuf.h"

static apr_status_t sbuf_cleanup(void *data);

StrBuf *
sbuf_make(apr_pool_t *pool)
{
    StrBuf *sbuf = ol_alloc(sizeof(*sbuf));
    sbuf->pool = pool;
    sbuf->data = ol_alloc(256);
    sbuf->max_len = 256;
    sbuf_reset(sbuf);

    apr_pool_cleanup_register(sbuf->pool, sbuf, sbuf_cleanup,
                              apr_pool_cleanup_null);

    return sbuf;
}

static apr_status_t
sbuf_cleanup(void *data)
{
    StrBuf *sbuf = (StrBuf *) data;

    ol_free(sbuf->data);
    ol_free(sbuf);

    return APR_SUCCESS;
}

void
sbuf_reset(StrBuf *sbuf)
{
    sbuf->len = 0;
    sbuf_reset_pos(sbuf);
}

void
sbuf_reset_pos(StrBuf *sbuf)
{
    sbuf->pos = 0;
}

/*
 * Return a copy of the current content of the StrBuf allocated from "pool",
 * plus a NUL-terminator. Note that we can do much better than apr_pstrdup()
 * for long strings, because we know the string's length in advance.
 */
char *
sbuf_dup(StrBuf *sbuf, apr_pool_t *pool)
{
    char *result;

    result = apr_palloc(pool, sbuf->len + 1);
    memcpy(result, sbuf->data, sbuf->len);
    result[sbuf->len] = '\0';

    return result;
}

/*
 * Note that we don't include the NUL-terminator in the StrBuf
 */
void
sbuf_append(StrBuf *sbuf, const char *str)
{
    sbuf_append_data(sbuf, str, strlen(str));
}

void
sbuf_append_data(StrBuf *sbuf, const char *data, apr_size_t len)
{
    sbuf_enlarge(sbuf, len);
    memcpy(sbuf->data + sbuf->len, data, len);
    sbuf->len += len;
}

void
sbuf_appendf(StrBuf *sbuf, const char *fmt, ...)
{
    FAIL();
}

/*
 * Equivalent to sbuf_append(sbuf, "c"), but marginally faster. This would
 * be a feasible candidate for inlining.
 */
void
sbuf_append_char(StrBuf *sbuf, char c)
{
    sbuf_enlarge(sbuf, 1);
    sbuf->data[sbuf->len] = c;
    sbuf->len++;
}

/*
 * Ensure that the buffer can hold "more_bytes + 1" more bytes; that is,
 * "more_bytes" does not include the terminating NUL.
 *
 * XXX: we don't bother checking for integer overflow.
 */
void
sbuf_enlarge(StrBuf *sbuf, int more_bytes)
{
    int new_max_len;
    int new_alloc_sz;

    ASSERT(more_bytes >= 0);

    new_max_len = sbuf->len + more_bytes;
    if (new_max_len <= sbuf->max_len)
        return;         /* Enough space already */

    /*
     * To avoid too much malloc() traffic, we want to at least double the
     * current value of "max_len".
     */
    new_alloc_sz = 2 * sbuf->max_len;
    while (new_alloc_sz < new_max_len)
        new_alloc_sz *= 2;

    sbuf->data = ol_realloc(sbuf->data, new_alloc_sz);
    sbuf->max_len = new_alloc_sz;
}

void
sbuf_socket_recv(StrBuf *sbuf, apr_socket_t *sock, apr_size_t len)
{
    sbuf_enlarge(sbuf, len);
    socket_recv_data(sock, sbuf->data + sbuf->len, len);
    sbuf->len += len;
}
