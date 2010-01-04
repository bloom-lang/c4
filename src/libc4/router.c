#include <apr_hash.h>

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
#include "util/list.h"
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
    WI_PROGRAM
} WorkItemKind;

typedef struct WorkItem
{
    WorkItemKind kind;

    /* WI_TUPLE: */
    Tuple *tuple;
    TableDef *tbl_def;

    /* WI_PROGRAM: */
    char *program_src;
} WorkItem;

static void router_enqueue(apr_queue_t *queue, WorkItem *wi);
static void compute_fixpoint(C4Router *router);

C4Router *
router_make(C4Runtime *c4, apr_queue_t *queue)
{
    C4Router *router;

    router = apr_pcalloc(c4->pool, sizeof(*router));
    router->c4 = c4;
    router->pool = c4->pool;
    router->op_chain_tbl = apr_hash_make(router->pool);
    router->route_buf = tuple_buf_make(4096, router->pool);
    router->net_buf = tuple_buf_make(1024, router->pool);
    router->ntuple_routed = 0;
    router->queue = queue;

    return router;
}

apr_queue_t *
router_get_queue(C4Router *router)
{
    return router->queue;
}

/*
 * Route a new tuple that belongs to the given table. If the tuple is remote
 * (i.e. the tuple's location specifier denotes an address other than the
 * local node address), then we enqueue the tuple in the network buffer.
 * Otherwise, we route the tuple locally (pass it to the appropriate
 * operator chains).
 */
void
router_install_tuple(C4Router *router, Tuple *tuple, TableDef *tbl_def)
{
#if 1
    c4_log(router->c4, "%s: %s (=> %s)",
           __func__, log_tuple(router->c4, tuple), tbl_def->name);
#endif
    if (tuple_is_remote(tuple, tbl_def, router->c4))
    {
        router_enqueue_net(router, tuple, tbl_def);
        return;
    }

    /* If the tuple is a duplicate, no need to route it */
    if (tbl_def->table->insert(tbl_def->table, tuple) == false)
        return;

    router_enqueue_internal(router, tuple, tbl_def);
}

static void
compute_fixpoint(C4Router *router)
{
    TupleBuf *route_buf = router->route_buf;
    TupleBuf *net_buf = router->net_buf;
    bool benchmark_done = false;

    /*
     * NB: We route tuples from route_buf in a FIFO manner, but that
     * is not necessarily the only choice.
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
        TupleBufEntry *ent;

        ent = &net_buf->entries[net_buf->start];
        net_buf->start++;

        network_send(router->c4->net, ent->tuple, ent->tbl_def);
        tuple_unpin(ent->tuple);
    }

    ASSERT(tuple_buf_is_empty(route_buf));
    tuple_buf_reset(route_buf);
    tuple_buf_reset(net_buf);
    apr_pool_clear(router->c4->tmp_pool);

    if (benchmark_done)
        exit(1);
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
                router_install_tuple(router, wi->tuple, wi->tbl_def);
                break;

            case WI_PROGRAM:
                route_program(router, wi->program_src);
                break;

            default:
                ERROR("Unrecognized WorkItem kind: %d", (int) wi->kind);
        }

        compute_fixpoint(router);
        workitem_destroy(wi);
    }
}

void
router_enqueue_program(apr_queue_t *queue, const char *src)
{
    WorkItem *wi;

    wi = ol_alloc0(sizeof(*wi));
    wi->kind = WI_PROGRAM;
    wi->program_src = ol_strdup(src);

    router_enqueue(queue, wi);
}

void
router_enqueue_tuple(apr_queue_t *queue, Tuple *tuple, TableDef *tbl_def)
{
    WorkItem *wi;

    /* The tuple is unpinned when the router is finished with it */
    tuple_pin(tuple);

    wi = ol_alloc0(sizeof(*wi));
    wi->kind = WI_TUPLE;
    wi->tuple = tuple;
    wi->tbl_def = tbl_def;

    router_enqueue(queue, wi);
}

/*
 * Enqueue a new WorkItem to be routed. The WorkItem will be routed in some
 * subsequent fixpoint. WorkItems are routed in the order in which they are
 * enqueued. If the queue is full, this function blocks until the router has
 * had a chance to catchup.
 */
static void
router_enqueue(apr_queue_t *queue, WorkItem *wi)
{
    while (true)
    {
        apr_status_t s = apr_queue_push(queue, wi);

        /* XXX: should propagate APR_EOF failure back to client */
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
 * fixpoint. Because we don't insert into the router's work queue, this
 * should typically be invoked by the router thread itself.
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
