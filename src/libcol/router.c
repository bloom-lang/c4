#include <apr_queue.h>
#include <apr_thread_proc.h>

#include "col-internal.h"
#include "net/network.h"
#include "operator/operator.h"
#include "planner/installer.h"
#include "router.h"
#include "util/list.h"

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

typedef enum WorkItemKind
{
    WI_TUPLE,
    WI_PLAN,
    WI_SHUTDOWN
} WorkItemKind;

typedef struct WorkItem
{
    WorkItemKind kind;
    union
    {
        Tuple *tuple;
        ProgramPlan *plan;
    } data;
} WorkItem;

static void * APR_THREAD_FUNC router_thread_start(apr_thread_t *thread, void *data);
static void router_enqueue(ColRouter *router, WorkItem *wi);

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

static List *
tuple_get_op_chains(Tuple *tuple, ColRouter *router)
{
    return NULL;
}

static void
route_tuple(Tuple *tuple, ColRouter *router)
{
    List *op_chains;
    ListCell *lc;

    op_chains = tuple_get_op_chains(tuple, router);
    foreach (lc, op_chains)
    {
        OpChain *op_chain = (OpChain *) lc_ptr(lc);
        Operator *start;

        start = op_chain->chain_start;
        start->invoke(start, tuple);
    }

    router->ntuple_routed++;
    tuple_unpin(tuple);
}

static void * APR_THREAD_FUNC
router_thread_start(apr_thread_t *thread, void *data)
{
    ColRouter *router = (ColRouter *) data;

    while (true)
    {
        apr_status_t s;
        WorkItem *wi;

        s = apr_queue_pop(router->queue, (void **) &wi);

        if (s == APR_EINTR)
            continue;
        if (s == APR_EOF)
            break;
        if (s != APR_SUCCESS)
            FAIL();

        switch (wi->kind)
        {
            case WI_TUPLE:
                route_tuple(wi->data.tuple, router);
                break;

            case WI_PLAN:
                install_plan(wi->data.plan, router->col);
                break;

            case WI_SHUTDOWN:
                break;

            default:
                ERROR("Unrecognized WorkItem kind: %d", (int) wi->kind);
        }

        ol_free(wi);
    }

    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}

void
router_enqueue_plan(ColRouter *router, ProgramPlan *plan)
{
    WorkItem *wi;

    wi = ol_alloc(sizeof(*wi));
    wi->kind = WI_PLAN;
    wi->data.plan = plan;

    router_enqueue(router, wi);
}

void
router_enqueue_tuple(ColRouter *router, Tuple *tuple)
{
    WorkItem *wi;

    /* The tuple is unpinned when the router is finished with it */
    tuple_pin(tuple);

    wi = ol_alloc(sizeof(*wi));
    wi->kind = WI_TUPLE;
    wi->data.tuple = tuple;

    router_enqueue(router, wi);
}

/*
 * Enqueue a new WorkItem to be routed. The WorkItem will be processed in
 * some subsequent fixpoint. WorkItems are routed in the order in which they
 * are enqueued. If the queue is full, this function blocks until the router
 * has had a chance to catchup.
 */
static void
router_enqueue(ColRouter *router, WorkItem *wi)
{
    while (true)
    {
        apr_status_t s = apr_queue_push(router->queue, wi);

        if (s == APR_EINTR)
            continue;
        if (s == APR_SUCCESS)
            break;

        FAIL();
    }
}
