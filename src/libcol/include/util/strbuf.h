#ifndef STRBUF_H
#define STRBUF_H

#include <apr_network_io.h>

/*
 * A StrBuf is an extensible string. "buf" holds the region of storage
 * currently allocated for the string. "len" holds the current logical
 * length of the buffer, not including the NUL terminator ("buf" is always
 * kept NUL-terminated). "maxlen" holds the amount of storage allocated
 * (maxlen - len - 1 is the amount of currently-free storage).
 *
 * Because APR pools don't provide an equivalent to either free() or
 * realloc(), we currently allocate the buffer's storage via malloc(), and
 * register a cleanup function with the pool specified in sbuf_make().
 *
 * Based on the StringInfo data type in PostgreSQL.
 */
typedef struct StrBuf
{
    apr_pool_t *pool;
    char *data;
    int len;
    int max_len;
} StrBuf;

StrBuf *sbuf_make(apr_pool_t *pool);
void sbuf_reset(StrBuf *sbuf);
void sbuf_enlarge(StrBuf *sbuf, int more_bytes);
char *sbuf_dup(StrBuf *sbuf, apr_pool_t *pool);

void sbuf_append(StrBuf *sbuf, const char *str);
void sbuf_appendf(StrBuf *sbuf, const char *fmt, ...)
    __attribute((format(printf, 2, 3)));
void sbuf_append_char(StrBuf *sbuf, char c);
void sbuf_append_data(StrBuf *sbuf, const char *data, apr_size_t len);

void sbuf_socket_recv(StrBuf *sbuf, apr_socket_t *sock, apr_size_t len);

#endif  /* STRBUF_H */
