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
    char *data;
    apr_size_t len;
    apr_size_t max_len;
    apr_size_t pos;
} StrBuf;

#define sbuf_data_avail(sbuf)   ((sbuf)->len - (sbuf)->pos)

StrBuf *sbuf_make(apr_pool_t *pool);
void sbuf_reset(StrBuf *sbuf);
void sbuf_reset_pos(StrBuf *sbuf);
void sbuf_enlarge(StrBuf *sbuf, apr_size_t more_bytes);
char *sbuf_dup(StrBuf *sbuf, apr_pool_t *pool);

void sbuf_append(StrBuf *sbuf, const char *str);
void sbuf_appendf(StrBuf *sbuf, const char *fmt, ...)
    __attribute((format(printf, 2, 3)));
void sbuf_append_char(StrBuf *sbuf, char c);
void sbuf_append_int16(StrBuf *sbuf, apr_uint16_t i);
void sbuf_append_int32(StrBuf *sbuf, apr_uint32_t i);
void sbuf_append_data(StrBuf *sbuf, const char *data, apr_size_t len);

unsigned char sbuf_read_char(StrBuf *sbuf);
apr_uint16_t sbuf_read_int16(StrBuf *sbuf);
apr_uint32_t sbuf_read_int32(StrBuf *sbuf);
void sbuf_read_data(StrBuf *sbuf, char *data, apr_size_t len);

bool sbuf_socket_recv(StrBuf *sbuf, apr_socket_t *sock,
                      apr_size_t len, bool *is_eof);
bool sbuf_socket_send(StrBuf *sbuf, apr_socket_t *sock, bool *is_eof);

#endif  /* STRBUF_H */
