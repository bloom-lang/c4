#include <apr_hash.h>
#include <apr_queue.h>
#include <apr_thread_cond.h>

#include "c4-internal.h"
#include "net/network.h"
#include "operator/operator.h"
#include "parser/parser.h"
#include "planner/installer.h"
#include "planner/planner.h"
#include "router.h"
#include "storage/sqlite.h"
#include "storage/table.h"
#include "types/catalog.h"
#include "util/dump_table.h"
#include "util/list.h"
#include "util/strbuf.h"
#include "util/tuple_buf.h"

struct C4Router
{
    C4Runtime *c4;
    apr_pool_t *pool;

    /* Map from table name => OpChainList */
    apr_hash_t *op_chain_tbl;

    /* Queue of to-be-performed external actions inserted by other threads */
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
    WI_DUMP_TABLE,
    WI_CALLBACK,
    WI_SHUTDOWN
} WorkItemKind;

typedef struct WorkItem
{
    WorkItemKind kind;

    /* WI_TUPLE: */
    Tuple *tuple;
    TableDef *tbl_def;

    /* WI_PROGRAM: */
    char *program_src;

    /* WI_DUMP_TABLE: */
    char *tbl_name;
    apr_thread_cond_t *cond;
    apr_thread_mutex_t *lock;
    StrBuf *buf;
    bool is_done;

    /* WI_CALLBACK */
    char *callback_tbl_name;
    C4TableCallback callback;
    void *callback_data;
} WorkItem;

static void router_enqueue(C4Router *router, WorkItem *wi);
static bool drain_queue(C4Router *router);

C4Router *
router_make(C4Runtime *c4)
{
    C4Router *router;
    apr_status_t s;

    router = apr_pcalloc(c4->pool, sizeof(*router));
    router->c4 = c4;
    router->pool = c4->pool;
    router->op_chain_tbl = apr_hash_make(router->pool);
    router->route_buf = tuple_buf_make(4096, router->pool);
    router->net_buf = tuple_buf_make(512, router->pool);
    router->ntuple_routed = 0;
    s = apr_queue_create(&router->queue, 512, router->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return router;
}

void
router_do_fixpoint(C4Router *router)
{
    TupleBuf *route_buf = router->route_buf;
    TupleBuf *net_buf = router->net_buf;
    bool benchmark_done = false;

    ASSERT(tuple_buf_is_empty(net_buf));

    /*
     * We route tuples from route_buf in a FIFO manner, but that is not
     * necessarily the only choice.
     */
    while (!tuple_buf_is_empty(route_buf))
    {
        Tuple *tuple;
        TableDef *tbl_def;
        OpChain *op_chain;

        tuple_buf_shift(route_buf, &tuple, &tbl_def);

        op_chain = tbl_def->op_chain_list->head;
        while (op_chain != NULL)
        {
            Operator *start = op_chain->chain_start;
            start->invoke(start, tuple);
            op_chain = op_chain->next;
        }

        tuple_unpin(tuple);
        router->ntuple_routed++;

        /* XXX: temporary */
        if (router->ntuple_routed >= 3000000)
        {
            benchmark_done = true;
            break;
        }
    }

    /* If we modified persistent storage, commit to disk */
    if (router->c4->sql->xact_in_progress)
        sqlite_commit_xact(router->c4->sql);

    /* Fixpoint is now considered to be "complete" */

    /* Enqueue any outbound network messages */
    while (!tuple_buf_is_empty(net_buf))
    {
        Tuple *tuple;
        TableDef *tbl_def;

        tuple_buf_shift(net_buf, &tuple, &tbl_def);
        network_send(router->c4->net, tuple, tbl_def);
        tuple_unpin(tuple);
    }

    /* Sending network messages should not cause more routing work */
    ASSERT(tuple_buf_is_empty(route_buf));
    apr_pool_clear(router->c4->tmp_pool);

    if (benchmark_done)
        exit(1);
}

/*
 * Route a new tuple that belongs to the given table. If "check_remote" is true
 * and the tuple is remote (i.e. the tuple's location specifier denotes an
 * address other than the local node address), then we enqueue the tuple in the
 * network buffer.  Otherwise, we insert the tuple into the specified table, and
 * then enqueue it to be routed in the next fixpoint.
 *
 * XXX: "check_remote" is a hack. The problem is that a node might have many
 * different addresses (due to multi-homing, IPv4 vs. IPv6, DNS aliases,
 * etc.). If we receive a network tuple that doesn't exactly match our local
 * node address, we can easily get into an infinite loop.
 */
void
router_install_tuple(C4Router *router, Tuple *tuple, TableDef *tbl_def,
                     bool check_remote)
{
#if 1
    c4_log(router->c4, "%s: %s (=> %s)",
           __func__, log_tuple(router->c4, tuple), tbl_def->name);
#endif
    if (check_remote && tuple_is_remote(tuple, tbl_def, router->c4))
    {
        router_enqueue_net(router, tuple, tbl_def);
        return;
    }

    /* If the tuple is a duplicate, no need to route it */
    if (tbl_def->table->insert(tbl_def->table, tuple) == false)
        return;

    table_invoke_callbacks(tbl_def, tuple);
    router_enqueue_internal(router, tuple, tbl_def);
}

static void
route_program(C4Router *router, const char *src)
{
    C4Runtime *c4 = router->c4;
    AstProgram *ast;
    ProgramPlan *plan;

    ast = parse_str(src, c4->tmp_pool, c4);
    plan = plan_program(ast, c4->tmp_pool, c4);
    install_plan(plan, c4->tmp_pool, c4);
}

static void
route_dump_table(C4Router *router, WorkItem *wi)
{
    apr_thread_mutex_lock(wi->lock);

    dump_table(router->c4, wi->tbl_name, wi->buf);
    wi->is_done = true;
    apr_thread_cond_signal(wi->cond);
    apr_thread_mutex_unlock(wi->lock);
}

static void
workitem_destroy(WorkItem *wi)
{
    switch (wi->kind)
    {
        case WI_TUPLE:
            tuple_unpin(wi->tuple);
            break;

        case WI_PROGRAM:
            ol_free(wi->program_src);
            break;

        /* XXX: hack. Let the client free the WorkItem */
        case WI_DUMP_TABLE:
            return;

        case WI_CALLBACK:
            ol_free(wi->callback_tbl_name);
            break;

        case WI_SHUTDOWN:
            break;

        default:
            ERROR("Unrecognized WorkItem kind: %d", (int) wi->kind);
    }

    ol_free(wi);
}

void
router_main_loop(C4Router *router)
{
    while (true)
    {
        network_poll(router->c4->net);
        if (!drain_queue(router))
            break;      /* Saw shutdown request */
    }
}

/*
 * Returns false if we saw a shutdown request, and true otherwise.
 */
static bool
drain_queue(C4Router *router)
{
    while (true)
    {
        apr_status_t s;
        WorkItem *wi;
        bool do_shutdown = false;

        s = apr_queue_trypop(router->queue, (void **) &wi);
        if (s == APR_EAGAIN)    /* Queue is empty */
            return true;
        if (s != APR_SUCCESS)
            FAIL_APR(s);

        switch (wi->kind)
        {
            case WI_TUPLE:
                router_install_tuple(router, wi->tuple, wi->tbl_def, true);
                break;

            case WI_PROGRAM:
                route_program(router, wi->program_src);
                break;

            case WI_DUMP_TABLE:
                route_dump_table(router, wi);
                break;

            case WI_CALLBACK:
                cat_register_callback(router->c4->cat, wi->callback_tbl_name,
                                      wi->callback, wi->callback_data);
                break;

            case WI_SHUTDOWN:
                do_shutdown = true;
                break;

            default:
                ERROR("Unrecognized WorkItem kind: %d", (int) wi->kind);
        }

        router_do_fixpoint(router);
        workitem_destroy(wi);
        if (do_shutdown)
            return false;
    }
}

void
runtime_enqueue_tuple(C4Runtime *c4, Tuple *tuple, TableDef *tbl_def)
{
    WorkItem *wi;

    /* The tuple is unpinned when the router is finished with it */
    tuple_pin(tuple);

    wi = ol_alloc0(sizeof(*wi));
    wi->kind = WI_TUPLE;
    wi->tuple = tuple;
    wi->tbl_def = tbl_def;

    router_enqueue(c4->router, wi);
}

void
runtime_enqueue_program(C4Runtime *c4, const char *src)
{
    WorkItem *wi;

    wi = ol_alloc0(sizeof(*wi));
    wi->kind = WI_PROGRAM;
    wi->program_src = ol_strdup(src);

    router_enqueue(c4->router, wi);
}

char *
runtime_enqueue_dump_table(C4Runtime *c4, const char *tbl_name,
                           apr_pool_t *pool)
{
    WorkItem *wi;
    char *result;

    wi = ol_alloc0(sizeof(*wi));
    wi->kind = WI_DUMP_TABLE;
    wi->is_done = false;
    wi->tbl_name = ol_strdup(tbl_name);
    wi->buf = sbuf_make(pool);

    apr_thread_mutex_create(&wi->lock, APR_THREAD_MUTEX_UNNESTED, pool);
    apr_thread_cond_create(&wi->cond, pool);
    apr_thread_mutex_lock(wi->lock);

    router_enqueue(c4->router, wi);

    /* may not make sense to do this here, but for now... */
    do {
        apr_thread_cond_wait(wi->cond, wi->lock);
    } while (!wi->is_done);

    result = wi->buf->data;
    apr_thread_cond_destroy(wi->cond);
    apr_thread_mutex_destroy(wi->lock);
    ol_free(wi->tbl_name);
    ol_free(wi);
    return result;
}

void
runtime_enqueue_callback(C4Runtime *c4, const char *tbl_name,
                         C4TableCallback callback, void *data)
{
    WorkItem *wi;

    wi = ol_alloc0(sizeof(*wi));
    wi->kind = WI_CALLBACK;
    wi->callback_tbl_name = ol_strdup(tbl_name);
    wi->callback = callback;
    wi->callback_data = data;

    router_enqueue(c4->router, wi);
}

void
runtime_enqueue_shutdown(C4Runtime *c4)
{
    WorkItem *wi;

    wi = ol_alloc0(sizeof(*wi));
    wi->kind = WI_SHUTDOWN;

    router_enqueue(c4->router, wi);
}

/*
 * Enqueue a new WorkItem to be routed. The WorkItem will be routed in some
 * subsequent fixpoint. WorkItems are routed in the order in which they are
 * enqueued. If the queue is full, this function blocks until the router has
 * had a chance to catchup.
 */
static void
router_enqueue(C4Router *router, WorkItem *wi)
{
    while (true)
    {
        apr_status_t s = apr_queue_push(router->queue, wi);

        /* XXX: should propagate APR_EOF failure back to client */
        if (s == APR_EINTR)
            continue;
        if (s == APR_SUCCESS)
            break;

        FAIL();
    }

    network_wakeup(router->c4->net);
}

/*
 * Get the list of OpChains associated with the given delta table. If the
 * OpChainList doesn't exist yet, it is created on the fly.
 */
OpChainList *
router_get_opchain_list(C4Router *router, const char *tbl_name)
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
router_add_op_chain(C4Router *router, OpChain *op_chain)
{
    OpChainList *opc_list;

    opc_list = apr_hash_get(router->op_chain_tbl, op_chain->delta_tbl->name,
                            APR_HASH_KEY_STRING);
    opchain_list_add(opc_list, op_chain);
}

/*
 * C4-internal: enqueue a new tuple to be routed within the CURRENT
 * fixpoint.
 */
void
router_enqueue_internal(C4Router *router, Tuple *tuple, TableDef *tbl_def)
{
    tuple_buf_push(router->route_buf, tuple, tbl_def);
}

/*
 * Enqueue a network output tuple to be sent at the end of the current
 * fixpoint.
 */
void
router_enqueue_net(C4Router *router, Tuple *tuple, TableDef *tbl_def)
{
    ASSERT(tbl_def->ls_colno != -1);

    tuple_buf_push(router->net_buf, tuple, tbl_def);
}
