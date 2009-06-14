#include "col-internal.h"
#include "router.h"

static void * APR_THREAD_FUNC router_thread_start(apr_thread_t *thread, void *data);

ColRouter *
router_make(ColInstance *col)
{
    apr_status_t s;
    apr_pool_t *pool;
    ColRouter *router;

    s = apr_pool_create(&pool, col->pool);
    if (s != APR_SUCCESS)
        FAIL();

    router = apr_pcalloc(pool, sizeof(*router));
    router->col = col;
    router->pool = pool;

    return router;
}

void
router_destroy(ColRouter *router)
{
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
        ;
    }

    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}

