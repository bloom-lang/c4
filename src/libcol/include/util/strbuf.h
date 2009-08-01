#ifndef STRBUF_H
#define STRBUF_H

#include <apr_network_io.h>

/*
 * A StrBuf is an extensible string buffer. "data" holds the region of
 * storage currently allocated for the string. "len" holds the current
 * logical length of the buffer. "maxlen" holds the amount of storage
 * allocated (maxlen - len is the amount of currently-free storage). Note
 * that StrBuf is not required to be NUL-terminated, and can be used to
 * store binary data.
 *
 * In addition to append operations, StrBuf also allows "relative" read
 * operations: that is, it records a current "position", and interprets the
 * read request relative to that position.
 *
 * Because APR pools don't provide an equivalent to either free() or
 * realloc(), we currently allocate the buffer's storage via malloc(), and
 * register a cleanup function with the pool specified in sbuf_make().
 *
 * Originally based on the StringInfo data type in PostgreSQL.
 */
typedef struct StrBuf
{
    apr_pool_t *pool;
    char *data;
    int len;
    int max_len;
    int pos;
} StrBuf;

StrBuf *sbuf_make(apr_pool_t *pool);
void sbuf_reset(StrBuf *sbuf);
void sbuf_reset_pos(StrBuf *sbuf);
void sbuf_enlarge(StrBuf *sbuf, int more_bytes);
char *sbuf_dup(StrBuf *sbuf, apr_pool_t *pool);

void sbuf_append(StrBuf *sbuf, const char *str);
void sbuf_appendf(StrBuf *sbuf, const char *fmt, ...)
    __attribute((format(printf, 2, 3)));
void sbuf_append_char(StrBuf *sbuf, char c);
void sbuf_append_data(StrBuf *sbuf, const char *data, apr_size_t len);

void sbuf_socket_recv(StrBuf *sbuf, apr_socket_t *sock, apr_size_t len);

#endif  /* STRBUF_H */
