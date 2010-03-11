#include "c4-internal.h"
#include "util/tuple_pool.h"

typedef struct FreeListElem
{
    struct FreeListElem *next_free;
} FreeListElem;

struct TuplePool
{
    apr_pool_t *pool;
    FreeListElem *free_head;
    apr_size_t elem_size;

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

    /* Next element in list of tuple pools maintained by the TuplePoolMgr */
    TuplePool *next;
};

struct TuplePoolMgr
{
    TuplePool *head;
    apr_pool_t *pool;
};

#define INITIAL_TPOOL_SIZE 64

static TuplePool *
make_tuple_pool(apr_size_t elem_size, TuplePoolMgr *tpool_mgr)
{
    TuplePool *tpool;

    tpool = apr_palloc(tpool_mgr->pool, sizeof(*tpool));
    tpool->pool = tpool_mgr->pool;
    tpool->free_head = NULL;
    /* XXX: Ensure this is word-aligned */
    tpool->elem_size = elem_size;
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

void *
tuple_pool_loan(TuplePool *tpool)
{
    void *result;

    /*
     * Use the free list if there are any tuples in it. We return the
     * most-recently inserted element of the free list, on the theory that
     * it is most likely to be "hot" cache-wise.
     */
    if (tpool->nfree > 0)
    {
        result = tpool->free_head;
        tpool->free_head = tpool->free_head->next_free;
        tpool->nfree--;
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
                                      alloc_size * tpool->elem_size);
        tpool->nalloc_total = alloc_size;
        tpool->nalloc_unused = alloc_size;
        tpool->ntotal += alloc_size;
    }

    result = (void *) tpool->raw_alloc;
    tpool->raw_alloc += tpool->elem_size;
    tpool->nalloc_unused--;

    return result;
}

void
tuple_pool_return(TuplePool *tpool, void *ptr)
{
    FreeListElem *new_elem = (FreeListElem *) ptr;

    tpool->nfree++;
    new_elem->next_free = tpool->free_head;
    tpool->free_head = new_elem;
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
get_tuple_pool(TuplePoolMgr *tpool_mgr, apr_size_t elem_size)
{
    TuplePool *tpool;

    for (tpool = tpool_mgr->head; tpool != NULL; tpool = tpool->next)
    {
        if (tpool->elem_size == elem_size)
            return tpool;
    }

    return make_tuple_pool(elem_size, tpool_mgr);
}
