#include <apr_queue.h>
#include <apr_thread_proc.h>
#include <stdlib.h>

#include "col-internal.h"
#include "net/network.h"
#include "router.h"

struct ColRouter
{
    ColInstance *col;
    apr_pool_t *pool;

    /* Thread info for router thread */
    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;

    /* Queue of to-be-routed tuples; accessed by other threads */
    apr_queue_t *queue;

    int ntuple_routed;
};

static void * APR_THREAD_FUNC router_thread_start(apr_thread_t *thread, void *data);

ColRouter *
router_make(ColInstance *col)
{
    apr_status_t s;
    apr_pool_t *pool;
    ColRouter *router;

    pool = make_subpool(col->pool);
    router = apr_pcalloc(pool, sizeof(*router));
    router->col = col;
    router->pool = pool;
    router->ntuple_routed = 0;

    s = apr_queue_create(&router->queue, 128, router->pool);
    if (s != APR_SUCCESS)
        FAIL();

    return router;
}

void
router_destroy(ColRouter *router)
{
    apr_status_t s;

    s = apr_queue_term(router->queue);
    if (s != APR_SUCCESS)
        FAIL();

    apr_pool_destroy(router->pool);
}

void
router_start(ColRouter *router)
{
    apr_status_t s;

    s = apr_threadattr_create(&router->thread_attr, router->pool);
    if (s != APR_SUCCESS)
        FAIL();

    s = apr_thread_create(&router->thread, router->thread_attr,
                          router_thread_start, router, router->pool);
    if (s != APR_SUCCESS)
        FAIL();
}

static void * APR_THREAD_FUNC
router_thread_start(apr_thread_t *thread, void *data)
{
    ColRouter *router = (ColRouter *) data;

    while (true)
    {
        apr_status_t s;
        Tuple *tuple;

        s = apr_queue_pop(router->queue, (void **) &tuple);

        if (s == APR_EINTR)
            continue;
        if (s == APR_EOF)
            break;
        if (s != APR_SUCCESS)
            FAIL();

        /* Route the new tuple */
        network_send(router->col->net, router->col->target_loc_spec, NULL);
        router->ntuple_routed++;

        if (router->ntuple_routed == 100000)
            exit(0);
    }

    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}

/*
 * Enqueue a new tuple to be routed. The tuple will be routed in some
 * subsequent fixpoint. Tuples are routed in the order in which they are
 * enqueued. If the queue is full, this function blocks until the router has
 * had a chance to catchup.
 */
void
router_enqueue(ColRouter *router, Tuple *tuple)
{
    while (true)
    {
        apr_status_t s = apr_queue_push(router->queue, tuple);

        if (s == APR_EINTR)
            continue;
        if (s == APR_SUCCESS)
            break;

        FAIL();
    }
}
