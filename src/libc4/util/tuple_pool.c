#include "c4-internal.h"
#include "types/schema.h"
#include "types/tuple.h"
#include "util/tuple_pool.h"

#define INITIAL_TPOOL_SIZE 64

TuplePool *
make_tuple_pool(Schema *schema, apr_pool_t *pool)
{
    TuplePool *tpool;
    apr_status_t s;

    tpool = apr_palloc(pool, sizeof(*tpool));
    tpool->pool = pool;
    if ((s = apr_thread_mutex_create(&tpool->lock,
                                     APR_THREAD_MUTEX_DEFAULT,
                                     tpool->pool)) != APR_SUCCESS)
        FAIL_APR(s);
    tpool->schema = schema;
    tpool->free_head = NULL;
    /* XXX: Ensure this is word-aligned */
    tpool->tuple_size = schema_get_tuple_size(schema);
    tpool->ntotal = 0;
    tpool->nfree = 0;
    tpool->nalloc_unused = 0;
    tpool->nalloc_total = 0;
    tpool->raw_alloc = NULL;

    return tpool;
}

Tuple *
tuple_pool_loan(TuplePool *tpool)
{
    Tuple *result;
    apr_status_t s;

    if ((s = apr_thread_mutex_lock(tpool->lock)) != APR_SUCCESS)
        FAIL_APR(s);

    /*
     * Use the free list if there are any tuples in it. We return the
     * most-recently inserted element of the free list, on the theory that
     * it is most likely to be "hot" cache-wise.
     */
    if (tpool->nfree > 0)
    {
        result = tpool->free_head;
        tpool->free_head = result->ptr.next_free;
        tpool->nfree--;

        result->ptr.schema = tpool->schema;
        result->refcount = 1;
        if ((s = apr_thread_mutex_unlock(tpool->lock)) != APR_SUCCESS)
            FAIL_APR(s);
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
    result->ptr.schema = tpool->schema;
    result->refcount = 1;
    tpool->raw_alloc += tpool->tuple_size;
    tpool->nalloc_unused--;

    if ((s = apr_thread_mutex_unlock(tpool->lock)) != APR_SUCCESS)
        FAIL_APR(s);
    return result;
}

void
tuple_pool_return(TuplePool *tpool, Tuple *tuple)
{
    ASSERT(tuple_get_schema(tuple) == tpool->schema);
    ASSERT(tuple->refcount == 0);

    tpool->nfree++;
    tuple->ptr.next_free = tpool->free_head;
    tpool->free_head = tuple;
}