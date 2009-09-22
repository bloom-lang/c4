#include <apr_hash.h>
#include <apr_queue.h>
#include <apr_thread_proc.h>

#include "col-internal.h"
#include "net/network.h"
#include "operator/operator.h"
#include "parser/parser.h"
#include "planner/installer.h"
#include "planner/planner.h"
#include "router.h"
#include "types/catalog.h"
#include "types/table.h"
#include "util/list.h"
#include "util/tuple_buf.h"

struct ColRouter
{
    ColInstance *col;
    apr_pool_t *pool;

    /* Map from table name => OpChainList */
    apr_hash_t *op_chain_tbl;

    /* Thread info for router thread */
    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;

    /* Queue of to-be-routed tuples inserted by clients */
    apr_queue_t *queue;

    /* Derived tuples computed within the current fixpoint; to-be-routed */
    TupleBuf *route_buf;
    /* Pending network output tuples computed within current fixpoint */
    TupleBuf *net_buf;

    int ntuple_routed;
};

typedef enum WorkItemKind
{
    WI_TUPLE,
    WI_PROGRAM,
    WI_SHUTDOWN
} WorkItemKind;

typedef struct WorkItem
{
    WorkItemKind kind;

    /* WI_TUPLE: */
    Tuple *tuple;
    char *tbl_name;

    /* WI_PROGRAM: */
    char *program_src;
} WorkItem;

static void * APR_THREAD_FUNC router_thread_start(apr_thread_t *thread, void *data);
static void router_enqueue(ColRouter *router, WorkItem *wi);
static void compute_fixpoint(ColRouter *router);

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
    router->op_chain_tbl = apr_hash_make(pool);
    router->route_buf = tuple_buf_make(4096, pool);
    router->net_buf = tuple_buf_make(1024, pool);
    router->ntuple_routed = 0;

    s = apr_queue_create(&router->queue, 256, router->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return router;
}

void
router_destroy(ColRouter *router)
{
    apr_status_t s;

    s = apr_queue_term(router->queue);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    apr_pool_destroy(router->pool);
}

void
router_start(ColRouter *router)
{
    apr_status_t s;

    s = apr_threadattr_create(&router->thread_attr, router->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_thread_create(&router->thread, router->thread_attr,
                          router_thread_start, router, router->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

/*
 * Route a new tuple that belongs to the given table. If the tuple is remote
 * (i.e. the tuple's location specifier denotes an address other than the
 * local node address), then we enqueue the tuple in the network buffer.
 * Otherwise, we route the tuple locally (pass it to the appropriate
 * operator chains).
 */
void
router_install_tuple(ColRouter *router, Tuple *tuple, TableDef *tbl_def)
{
#if 0
    col_log(router->col, "%s: %s (=> %s)",
            __func__, log_tuple(router->col, tuple), tbl_def->name);
#endif
    if (tuple_is_remote(tuple, tbl_def, router->col))
    {
        router_enqueue_net(router, tuple, tbl_def);
        return;
    }

    /* If the tuple is a duplicate, no need to route it */
    if (table_insert(tbl_def->table, tuple) == false)
        return;

    router_enqueue_internal(router, tuple, tbl_def);
}

static void
compute_fixpoint(ColRouter *router)
{
    TupleBuf *route_buf = router->route_buf;
    TupleBuf *net_buf = router->net_buf;

    /*
     * NB: We route tuples from route_buf in a FIFO manner, but that is not
     * necessarily the only choice.
     */
    while (!tuple_buf_is_empty(route_buf))
    {
        TupleBufEntry *ent;
        OpChain *op_chain;

        ent = &route_buf->entries[route_buf->start];
        route_buf->start++;

        op_chain = ent->tbl_def->op_chain_list->head;
        while (op_chain != NULL)
        {
            Operator *start = op_chain->chain_start;
            start->invoke(start, ent->tuple);
            op_chain = op_chain->next;
        }

        tuple_unpin(ent->tuple);
        router->ntuple_routed++;
    }

    while (!tuple_buf_is_empty(net_buf))
    {
        TupleBufEntry *ent;

        ent = &net_buf->entries[net_buf->start];
        net_buf->start++;

        network_send(router->col->net, ent->tuple, ent->tbl_def);
        tuple_unpin(ent->tuple);
    }

    ASSERT(tuple_buf_is_empty(route_buf));
    tuple_buf_reset(route_buf);
    tuple_buf_reset(net_buf);

    /* XXX: temporary */
    if (router->ntuple_routed >= 3000000)
        exit(0);
}

static void
route_tuple(ColRouter *router, Tuple *tuple, const char *tbl_name)
{
    TableDef *tbl_def;

    tbl_def = cat_get_table(router->col->cat, tbl_name);
    router_install_tuple(router, tuple, tbl_def);
}

static void
route_program(ColRouter *router, const char *src)
{
    ColInstance *col = router->col;
    apr_pool_t *program_pool;
    AstProgram *ast;
    ProgramPlan *plan;

    program_pool = make_subpool(col->pool);

    ast = parse_str(src, program_pool, col);
    plan = plan_program(ast, program_pool, col);
    install_plan(plan, program_pool, col);

    apr_pool_destroy(program_pool);
}

static void
workitem_destroy(WorkItem *wi)
{
    switch (wi->kind)
    {
        case WI_TUPLE:
            tuple_unpin(wi->tuple);
            ol_free(wi->tbl_name);
            break;

        case WI_PROGRAM:
            ol_free(wi->program_src);
            break;

        case WI_SHUTDOWN:
            break;

        default:
            ERROR("Unrecognized WorkItem kind: %d", (int) wi->kind);
    }

    ol_free(wi);
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
            FAIL_APR(s);

        switch (wi->kind)
        {
            case WI_TUPLE:
                route_tuple(router, wi->tuple, wi->tbl_name);
                break;

            case WI_PROGRAM:
                route_program(router, wi->program_src);
                break;

            case WI_SHUTDOWN:
                /* XXX: TODO */
                break;

            default:
                ERROR("Unrecognized WorkItem kind: %d", (int) wi->kind);
        }

        compute_fixpoint(router);
        workitem_destroy(wi);
    }

    apr_thread_exit(thread, APR_SUCCESS);
    return NULL;        /* Return value ignored */
}

void
router_enqueue_program(ColRouter *router, const char *src)
{
    WorkItem *wi;

    wi = ol_alloc0(sizeof(*wi));
    wi->kind = WI_PROGRAM;
    wi->program_src = ol_strdup(src);

    router_enqueue(router, wi);
}

void
router_enqueue_tuple(ColRouter *router, Tuple *tuple,
                     const char *tbl_name)
{
    WorkItem *wi;

#if 0
    col_log(router->col, "%s: %s",
            __func__, log_tuple(router->col, tuple));
#endif

    /* The tuple is unpinned when the router is finished with it */
    tuple_pin(tuple);

    wi = ol_alloc0(sizeof(*wi));
    wi->kind = WI_TUPLE;
    wi->tuple = tuple;
    wi->tbl_name = ol_strdup(tbl_name);

    router_enqueue(router, wi);
}

/*
 * Enqueue a new WorkItem to be routed. The WorkItem will be routed in some
 * subsequent fixpoint. WorkItems are routed in the order in which they are
 * enqueued. If the queue is full, this function blocks until the router has
 * had a chance to catchup.
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

/*
 * Get the list of OpChains associated with the given delta table. If the
 * OpChainList doesn't exist yet, it is created on the fly.
 */
OpChainList *
router_get_opchain_list(ColRouter *router, const char *tbl_name)
{
    OpChainList *result;

    result = apr_hash_get(router->op_chain_tbl, tbl_name,
                          APR_HASH_KEY_STRING);
    if (result == NULL)
    {
        result = opchain_list_make(router->pool);
        apr_hash_set(router->op_chain_tbl, tbl_name,
                     APR_HASH_KEY_STRING, result);
    }

    return result;
}

void
router_add_op_chain(ColRouter *router, OpChain *op_chain)
{
    OpChainList *opc_list;

    opc_list = apr_hash_get(router->op_chain_tbl, op_chain->delta_tbl->name,
                            APR_HASH_KEY_STRING);
    opchain_list_add(opc_list, op_chain);
}

/*
 * COL-internal: enqueue a new tuple to be routed within the CURRENT
 * fixpoint. Because we don't insert into the router's work queue, this
 * should typically be invoked by the router thread itself.
 */
void
router_enqueue_internal(ColRouter *router, Tuple *tuple, TableDef *tbl_def)
{
    tuple_buf_push(router->route_buf, tuple, tbl_def);
}

/*
 * Enqueue a network output tuple to be sent at the end of the current
 * fixpoint.
 */
void
router_enqueue_net(ColRouter *router, Tuple *tuple, TableDef *tbl_def)
{
    ASSERT(tbl_def->ls_colno != -1);

    tuple_buf_push(router->net_buf, tuple, tbl_def);
}
