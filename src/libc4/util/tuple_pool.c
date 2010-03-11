#include "c4-internal.h"
#include "types/tuple.h"
#include "util/tuple_pool.h"

#define INITIAL_TPOOL_SIZE 64

static TuplePool *
make_tuple_pool(apr_size_t alloc_sz, TuplePoolMgr *tpool_mgr)
{
    TuplePool *tpool;

    tpool = apr_palloc(tpool_mgr->pool, sizeof(*tpool));
    tpool->pool = tpool_mgr->pool;
    tpool->free_head = NULL;
    /* XXX: Ensure this is word-aligned */
    tpool->tuple_size = alloc_sz;
    tpool->ntotal = 0;
    tpool->nfree = 0;
    tpool->nalloc_unused = 0;
    tpool->nalloc_total = 0;
    tpool->raw_alloc = NULL;

    /* Add to linked list of tuple pools */
    tpool->next = tpool_mgr->head;
    tpool_mgr->head = tpool;

    return tpool;
}

Tuple *
tuple_pool_loan(TuplePool *tpool)
{
    Tuple *result;

    /*
     * Use the free list if there are any tuples in it. We return the
     * most-recently inserted element of the free list, on the theory that
     * it is most likely to be "hot" cache-wise.
     */
    if (tpool->nfree > 0)
    {
        result = tpool->free_head;
        tpool->free_head = result->u.next_free;
        tpool->nfree--;

        result->u.refcount = 1;
        return result;
    }

    ASSERT(tpool->free_head == NULL);

    if (tpool->nalloc_unused == 0)
    {
        int alloc_size;

        if (tpool->nalloc_total == 0)
            alloc_size = INITIAL_TPOOL_SIZE;
        else
            alloc_size = tpool->nalloc_total * 2;

        tpool->raw_alloc = apr_palloc(tpool->pool,
                                      alloc_size * tpool->tuple_size);
        tpool->nalloc_total = alloc_size;
        tpool->nalloc_unused = alloc_size;
        tpool->ntotal += alloc_size;
    }

    result = (Tuple *) tpool->raw_alloc;
    result->u.refcount = 1;
    tpool->raw_alloc += tpool->tuple_size;
    tpool->nalloc_unused--;

    return result;
}

void
tuple_pool_return(TuplePool *tpool, Tuple *tuple)
{
    ASSERT(tuple->u.refcount == 0);

    tpool->nfree++;
    tuple->u.next_free = tpool->free_head;
    tpool->free_head = tuple;
}

TuplePoolMgr *
tpool_mgr_make(apr_pool_t *pool)
{
    TuplePoolMgr *result;

    result = apr_palloc(pool, sizeof(*result));
    result->head = NULL;
    result->pool = pool;

    return result;
}

/*
 * This is not efficient, but we don't expect that it will be called in the
 * critical path.
 */
TuplePool *
get_tuple_pool(TuplePoolMgr *tpool_mgr, apr_size_t alloc_sz)
{
    TuplePool *tpool;

    for (tpool = tpool_mgr->head; tpool != NULL; tpool = tpool->next)
    {
        if (tpool->tuple_size == alloc_sz)
            return tpool;
    }

    return make_tuple_pool(alloc_sz, tpool_mgr);
}
