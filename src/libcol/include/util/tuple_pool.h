#ifndef TUPLE_POOL_H
#define TUPLE_POOL_H

struct Schema;
struct Tuple;

/*
 * A TuplePool is a memory allocator that is specialized for Tuples with a
 * particular schema. Because all such Tuples have the same size, we can do
 * better than retail malloc() and free(): we grab large chunks of memory
 * via apr_palloc() and then use them to hold many individual Tuples. There
 * is also a simple freelist: when a Tuple is returned to the pool, it is
 * then eligible to be reused as the storage for a newly-allocated Tuple.
 * Note that we make no attempt to avoid fragmentation or return the contents
 * of the freelist to the OS if it grows large.
 *
 * XXX: We really only need a TuplePool for each distinct tuple size.
 */
typedef struct TuplePool
{
    apr_pool_t *pool;
    struct Schema *schema;
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
} TuplePool;

TuplePool *make_tuple_pool(struct Schema *schema, apr_pool_t *pool);
struct Tuple *tuple_pool_loan(TuplePool *tpool);
void tuple_pool_return(TuplePool *tpool, struct Tuple *tuple);

#endif  /* TUPLE_POOL_H */
