#ifndef TUPLE_BUF_H
#define TUPLE_BUF_H

/*
 * TupleBufs are used to hold collections of Tuples; right now, we use them
 * to accumulate to-be-routed derived tuples and pending network output
 * tuples in the midst of a fixpoint.
 *
 * XXX: Storing a TableDef with each entry is unfortunate, because in many
 * cases the number of distinct TableDefs in a large TupleBuf will be
 * small. For now seems better to trade memory consumption for faster
 * routing performance, but this tradeoff should be examined.
 */
typedef struct TupleBufEntry
{
    Tuple *tuple;
    TableDef *tbl_def;
} TupleBufEntry;

typedef struct TupleBuf
{
    int size;           /* # of entries allocated in this buffer */
    int start;          /* Index of first valid (to-be-routed) entry */
    int end;            /* Index of last valid (to-be-routed) entry */
    TupleBufEntry *entries;
} TupleBuf;

#define tuple_buf_is_empty(buf)     ((buf)->start == (buf)->end)
#define tuple_buf_size(buf)         ((buf)->end - (buf)->start)

TupleBuf *tuple_buf_make(int size, apr_pool_t *pool);
void tuple_buf_reset(TupleBuf *buf);
void tuple_buf_push(TupleBuf *buf, Tuple *tuple, TableDef *tbl_def);
void tuple_buf_shift(TupleBuf *buf, Tuple **tuple, TableDef **tbl_def);

#endif  /* TUPLE_BUF_H */
