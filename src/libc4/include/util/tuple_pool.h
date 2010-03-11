#ifndef TUPLE_POOL_H
#define TUPLE_POOL_H

/*
 * A TuplePool is a memory allocator that is specialized for Tuples of a
 * particular size. We can do better than retail malloc() and free(): we grab
 * large chunks of memory via apr_palloc() and then use them to hold many
 * individual Tuples. There is also a simple freelist: when a Tuple is returned
 * to the pool, it is then eligible to be reused as the storage for a
 * newly-allocated Tuple.  Note that we make no attempt to avoid fragmentation
 * or return the contents of the freelist to the OS if it grows large.
 */
typedef struct TuplePool TuplePool;

void *tuple_pool_loan(TuplePool *tpool);
void tuple_pool_return(TuplePool *tpool, void *ptr);

/*
 * A TuplePoolMgr manages the set of TuplePools in a given C4 instance. It is
 * just used as a means to share TuplePools of the same size among different
 * call sites.
 */
typedef struct TuplePoolMgr TuplePoolMgr;

TuplePoolMgr *tpool_mgr_make(apr_pool_t *pool);
TuplePool *get_tuple_pool(TuplePoolMgr *tpool_mgr, apr_size_t elem_size);

#endif  /* TUPLE_POOL_H */
