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
#include "runtime.h"
#include "storage/sqlite.h"
#include "storage/table.h"
#include "timer.h"
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

    /* Queue of to-be-consumed input events inserted by other threads */
    apr_queue_t *queue;

    /* Inserts and deletes computed within current fixpoint; to-be-routed */
    TupleBuf *insert_buf;
    TupleBuf *delete_buf;
    bool routing_deletes;       /* Are we currently routing from delete_buf? */

    /* Pending network output tuples computed within current fixpoint */
    TupleBuf *net_buf;
};

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
    router->insert_buf = tuple_buf_make(4096, router->pool);
    router->delete_buf = tuple_buf_make(512, router->pool);
    router->routing_deletes = false;
    router->net_buf = tuple_buf_make(512, router->pool);
    s = apr_queue_create(&router->queue, 512, router->pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return router;
}

/*
 * We route tuples from the buffer in a FIFO manner, but that is not necessarily
 * the only choice.
 */
static void
route_tuple_buf(C4Router *router, TupleBuf *buf, bool is_delete)
{
    while (!tuple_buf_is_empty(buf))
    {
        Tuple *tuple;
        TableDef *tbl_def;
        OpChain *op_chain;
        bool route_tuple;

        tuple_buf_shift(buf, &tuple, &tbl_def);

#if 0
        c4_log(router->c4, "%s: %s %s (=> %s)",
               __func__, is_delete ? "delete" : "insert",
               log_tuple(router->c4, tuple, tbl_def->schema),
               tbl_def->name);
#endif

        if (is_delete)
            route_tuple = tbl_def->table->delete(tbl_def->table, tuple);
        else
            route_tuple = tbl_def->table->insert(tbl_def->table, tuple);

        if (!route_tuple)
            return;

        op_chain = tbl_def->op_chain_list->head;
        while (op_chain != NULL)
        {
            Operator *start = op_chain->chain_start;

            if (op_chain->anti_chain)
                router->routing_deletes = !is_delete;
            else
                router->routing_deletes = is_delete;

            start->invoke(start, tuple);
            op_chain = op_chain->next;
        }

        tuple_unpin(tuple, tbl_def->schema);
    }
}

static void
router_do_fixpoint(C4Router *router)
{
    TupleBuf *net_buf = router->net_buf;

    ASSERT(tuple_buf_is_empty(net_buf));

    while (!tuple_buf_is_empty(router->insert_buf) ||
           !tuple_buf_is_empty(router->delete_buf))
    {
        route_tuple_buf(router, router->insert_buf, false);
        route_tuple_buf(router, router->delete_buf, true);
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
        tuple_unpin(tuple, tbl_def->schema);
    }

    /* Sending network messages should not cause more routing work */
    ASSERT(tuple_buf_is_empty(router->insert_buf));
    ASSERT(tuple_buf_is_empty(router->delete_buf));
    apr_pool_clear(router->c4->tmp_pool);
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
router_insert_tuple(C4Router *router, Tuple *tuple, TableDef *tbl_def,
                    bool check_remote)
{
#if 0
    c4_log(router->c4, "%s: %s (=> %s)",
           __func__, log_tuple(router->c4, tuple, tbl_def->schema),
           tbl_def->name);
#endif

    if (check_remote && tuple_is_remote(tuple, tbl_def, router->c4))
    {
        tuple_buf_push(router->net_buf, tuple, tbl_def);
        return;
    }

    table_invoke_callbacks(tuple, tbl_def, false);
    router_enqueue_internal(router, tuple, tbl_def);
}

void
router_delete_tuple(C4Router *router, Tuple *tuple, TableDef *tbl_def)
{
#if 0
    c4_log(router->c4, "%s: %s (=> %s)",
           __func__, log_tuple(router->c4, tuple, tbl_def->schema),
           tbl_def->name);
#endif

    table_invoke_callbacks(tuple, tbl_def, true);
    tuple_buf_push(router->delete_buf, tuple, tbl_def);
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

void
router_main_loop(C4Router *router)
{
    while (true)
    {
        apr_interval_time_t timeout;

        /* Fire any pending alarms */
        timer_invoke(router->c4->timer);

        timeout = timer_get_sleep_time(router->c4->timer);
        if (network_poll(router->c4->net, timeout))
            router_do_fixpoint(router);
        else
        {
            if (!drain_queue(router))
                break;      /* Saw shutdown request */
        }
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
            case WI_PROGRAM:
                route_program(router, wi->program_src);
                break;

            case WI_DUMP_TABLE:
                dump_table(router->c4, wi->tbl_name, wi->buf);
                break;

            case WI_CALLBACK:
                cat_register_callback(router->c4->cat, wi->cb_tbl_name,
                                      wi->cb_func, wi->cb_data);
                break;

            case WI_SHUTDOWN:
                do_shutdown = true;
                break;

            default:
                ERROR("Unrecognized WorkItem kind: %d", (int) wi->kind);
        }

        router_do_fixpoint(router);
        thread_sync_signal(wi->sync);
        if (do_shutdown)
            return false;
    }
}

void
runtime_enqueue_work(C4Runtime *c4, WorkItem *wi)
{
    router_enqueue(c4->router, wi);
    thread_sync_wait(wi->sync);
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
    tuple_buf_push(router->insert_buf, tuple, tbl_def);
}

bool
router_is_deleting(C4Router *router)
{
    return router->routing_deletes;
}
