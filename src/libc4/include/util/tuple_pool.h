#ifndef TUPLE_POOL_H
#define TUPLE_POOL_H

struct Tuple;

/*
 * A TuplePool is a memory allocator that is specialized for Tuples of a
 * particular size. We can do better than retail malloc() and free(): we grab
 * large chunks of memory via apr_palloc() and then use them to hold many
 * individual Tuples. There is also a simple freelist: when a Tuple is returned
 * to the pool, it is then eligible to be reused as the storage for a
 * newly-allocated Tuple.  Note that we make no attempt to avoid fragmentation
 * or return the contents of the freelist to the OS if it grows large.
 */
typedef struct TuplePool
{
    apr_pool_t *pool;
    struct Tuple *free_head;
    apr_size_t tuple_size;

    /*
     * The total number of tuples of this schema we've allocated space
     * for. This includes live tuples, free tuples, and tuples in
     * "nalloc_unused".
     */
    int ntotal;

    /*
     * The number of tuples currently on the freelist. This doesn't include
     * "nalloc_unused" below.
     */
    int nfree;

    /*
     * The current raw allocation that we're assigning tuples from. This is
     * used when we can't find a free tuple on the freelist. When the
     * allocation is exhausted, we allocate a new block from apr_palloc().
     */
    char *raw_alloc;
    int nalloc_unused;
    int nalloc_total;

    /* List of tuple pools maintained by the TuplePoolMgr */
    struct TuplePool *next;
} TuplePool;

struct Tuple *tuple_pool_loan(TuplePool *tpool);
void tuple_pool_return(TuplePool *tpool, struct Tuple *tuple);

typedef struct TuplePoolMgr
{
    TuplePool *head;
    apr_pool_t *pool;
} TuplePoolMgr;

TuplePoolMgr *tpool_mgr_make(apr_pool_t *pool);
TuplePool *get_tuple_pool(TuplePoolMgr *tpool_mgr, apr_size_t alloc_sz);

#endif  /* TUPLE_POOL_H */
