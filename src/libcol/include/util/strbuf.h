#ifndef STRBUF_H
#define STRBUF_H

/*
 * A StrBuf is an extensible string. "buf" holds the region of storage
 * currently allocated for the string. "len" holds the current logical
 * length of the buffer, not including the NUL terminator ("buf" is always
 * kept NUL-terminated). "maxlen" holds the amount of storage allocated
 * (maxlen - len - 1 is the amount of currently-free storage).
 *
 * Because APR pools are braindamaged and don't provide an equivalent to
 * either free() or realloc(), we currently allocate the buffer's storage
 * via malloc(), and register a cleanup function with the pool specified in
 * sbuf_make().
 *
 * Based on the StringInfo utility package in PostgreSQL.
 */
typedef struct StrBuf
{
    apr_pool_t *pool;
    char *buf;
    int len;
    int max_len;
} StrBuf;

StrBuf *sbuf_make(apr_pool_t *pool);
void sbuf_reset(StrBuf *sbuf);

void sbuf_append(StrBuf *sbuf, const char *str);
void sbuf_appendf(StrBuf *sbuf, const char *fmt, ...)
    __attribute((format(printf, 2, 3)));
void sbuf_append_char(StrBuf *sbuf, char c);

#endif  /* STRBUF_H */
