#include "c4-internal.h"
#include "util/tuple_buf.h"

static apr_status_t tuple_buf_cleanup(void *data);

TupleBuf *
tuple_buf_make(int size, apr_pool_t *pool)
{
    TupleBuf *buf;

    buf = apr_palloc(pool, sizeof(*buf));
    buf->size = size;
    buf->start = 0;
    buf->end = 0;
    buf->entries = ol_alloc(buf->size * sizeof(TupleBufEntry));

    apr_pool_cleanup_register(pool, buf, tuple_buf_cleanup,
                              apr_pool_cleanup_null);

    return buf;
}

static apr_status_t
tuple_buf_cleanup(void *data)
{
    TupleBuf *buf = (TupleBuf *) data;

    while (!tuple_buf_is_empty(buf))
    {
        Tuple *tuple;
        TableDef *tbl_def;

        tuple_buf_shift(buf, &tuple, &tbl_def);
        tuple_unpin(tuple, tbl_def->schema);
    }

    ol_free(buf->entries);
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
            buf->entries = ol_realloc(buf->entries,
                                      buf->size * sizeof(TupleBufEntry));
        }
    }

    ent = &buf->entries[buf->end];
    ent->tuple = tuple;
    ent->tbl_def = tbl_def;
    tuple_pin(ent->tuple);

    buf->end++;
}

/*
 * Remove the first element of the TupleBuf, returning its content via
 * the two out parameters. Note that we do NOT unpin the tuple: the
 * caller should do that when they are finished with it.
 */
void
tuple_buf_shift(TupleBuf *buf, Tuple **tuple, TableDef **tbl_def)
{
    TupleBufEntry *ent;

    ASSERT(!tuple_buf_is_empty(buf));
    ent = &buf->entries[buf->start];
    buf->start++;
    if (tuple_buf_is_empty(buf))
        tuple_buf_reset(buf);

    if (tuple)
        *tuple = ent->tuple;
    if (tbl_def)
        *tbl_def = ent->tbl_def;
}
