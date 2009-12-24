#include "c4-internal.h"
#include "util/socket.h"
#include "util/strbuf.h"

static apr_status_t sbuf_cleanup(void *data);
static bool sbuf_append_va(StrBuf *sbuf, const char *fmt, va_list args);

StrBuf *
sbuf_make(apr_pool_t *pool)
{
    StrBuf *sbuf = apr_palloc(pool, sizeof(*sbuf));
    sbuf->data = ol_alloc(256);
    sbuf->max_len = 256;
    sbuf_reset(sbuf);

    apr_pool_cleanup_register(pool, sbuf, sbuf_cleanup,
                              apr_pool_cleanup_null);

    return sbuf;
}

static apr_status_t
sbuf_cleanup(void *data)
{
    StrBuf *sbuf = (StrBuf *) data;

    ol_free(sbuf->data);
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
	for (;;)
	{
		va_list		args;
		bool		success;

		/* Try to format the data. */
		va_start(args, fmt);
		success = sbuf_append_va(sbuf, fmt, args);
		va_end(args);

		if (success)
			break;

		/* Double the buffer size and try again. */
        sbuf_enlarge(sbuf, sbuf->max_len);
	}    
}

/*
 * Attempt to format text data under the control of fmt (an sprintf-style
 * format string) and append it to the given StrBuf. If successful return
 * true; if not (because there's not enough space), return false without
 * modifying the StrBuf.  Typically the caller would call sbuf_enlarge() on
 * false return -- see sbuf_appendf() for example.
 *
 * XXX This API is ugly, but there seems no alternative given the C spec's
 * restrictions on what can portably be done with va_list arguments: you have
 * to redo va_start before you can rescan the argument list, and we can't do
 * that from here.
 */
static bool
sbuf_append_va(StrBuf *sbuf, const char *fmt, va_list args)
{
    int avail;
    int nprinted;

	/*
	 * If there's hardly any space, don't bother trying, just fail to make the
	 * caller enlarge the buffer first.
	 */
	avail = sbuf->max_len - sbuf->len;
	if (avail < 16)
		return false;

    nprinted = vsnprintf(sbuf->data + sbuf->len, avail, fmt, args);

	/*
	 * Note: some versions of vsnprintf return the number of chars actually
	 * stored, but at least one returns -1 on failure. Be conservative about
	 * believing whether the print worked.
	 */
	if (nprinted >= 0 && nprinted < avail)
	{
		/* Success.  Note nprinted does not include trailing null. */
		sbuf->len += nprinted;
		return true;
	}

    return false;
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
 * Ensure that the buffer can hold "more_bytes" more bytes.
 *
 * XXX: we don't bother checking for integer overflow.
 */
void
sbuf_enlarge(StrBuf *sbuf, apr_size_t more_bytes)
{
    apr_size_t new_max_len;
    apr_size_t new_alloc_sz;

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

unsigned char
sbuf_read_char(StrBuf *sbuf)
{
    unsigned char result;

    sbuf_read_data(sbuf, (char *) &result, sizeof(result));
    return result;
}

apr_uint16_t
sbuf_read_int16(StrBuf *sbuf)
{
    apr_uint16_t result;

    sbuf_read_data(sbuf, (char *) &result, sizeof(result));
    return result;
}

apr_uint32_t
sbuf_read_int32(StrBuf *sbuf)
{
    apr_uint32_t result;

    sbuf_read_data(sbuf, (char *) &result, sizeof(result));
    return result;
}

void
sbuf_read_data(StrBuf *sbuf, char *data, apr_size_t len)
{
    if (sbuf->pos + len > sbuf->len)
        FAIL();         /* Not enough data in the buffer */

    memcpy(data, sbuf->data + sbuf->pos, len);
    sbuf->pos += len;
}

void
sbuf_socket_recv(StrBuf *sbuf, apr_socket_t *sock,
                 apr_size_t len, bool *is_eof)
{
    sbuf_enlarge(sbuf, len);
    socket_recv_data(sock, sbuf->data + sbuf->len, len, is_eof);
    sbuf->len += len;
}
