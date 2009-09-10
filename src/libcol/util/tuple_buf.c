#include "col-internal.h"
#include "util/tuple_buf.h"

static apr_status_t tuple_buf_cleanup(void *data);

TupleBuf *
tuple_buf_make(apr_pool_t *pool)
{
    TupleBuf *buf;

    buf = ol_alloc(sizeof(*buf));
    buf->size = 4096;
    buf->start = 0;
    buf->end = 0;
    buf->entries = ol_alloc(sizeof(TupleBufEntry) * buf->size);

    apr_pool_cleanup_register(pool, buf, tuple_buf_cleanup,
                              apr_pool_cleanup_null);

    return buf;
}

static apr_status_t
tuple_buf_cleanup(void *data)
{
    TupleBuf *buf = (TupleBuf *) data;

    ol_free(buf->entries);
    ol_free(buf);
    return APR_SUCCESS;
}

void
tuple_buf_reset(TupleBuf *buf)
{
    buf->start = 0;
    buf->end = 0;
}

void
tuple_buf_push(TupleBuf *buf, Tuple *tuple, TableDef *tbl_def)
{
    TupleBufEntry *ent;

    if (buf->end == buf->size)
    {
        /*
         * If we've reached the end of the buffer, we need to make room for
         * more insertions. We can recover some space by moving the valid
         * buffer content back to the beginning of the buffer. If this is
         * impossible or not worth the effort, allocate a larger buffer.
         *
         * Our quick-and-dirty test for whether moving the buffer contents
         * is worthwhile is whether we'd regain >= 33% of the buffer space.
         * XXX: This could likely be improved.
         */
        if (buf->start >= (buf->size / 3))
        {
            int nvalid = buf->end - buf->start;

            memmove(buf->entries, &buf->entries[buf->start],
                    nvalid * sizeof(TupleBufEntry));
            buf->start = 0;
            buf->end = nvalid;
        }
        else
        {
            buf->size *= 2;
            buf->entries = ol_realloc(buf->entries, buf->size);
        }
    }

    ent = &buf->entries[buf->end];
    ent->tuple = tuple;
    ent->tbl_def = tbl_def;
    tuple_pin(ent->tuple);

    buf->end++;
}
