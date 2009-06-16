#include "col-internal.h"
#include "net/network.h"
#include "parser/parser.h"
#include "router.h"

void
col_initialize()
{
    apr_status_t s = apr_initialize();
    if (s != APR_SUCCESS)
        FAIL();
}

void
col_terminate()
{
    apr_terminate();
}

ColInstance *
col_make(int port)
{
    apr_status_t s;
    apr_pool_t *pool;
    ColInstance *col;

    s = apr_pool_create(&pool, NULL);
    if (s != APR_SUCCESS)
        FAIL();

    col = apr_pcalloc(pool, sizeof(*col));
    col->pool = pool;
    col->router = router_make(col);
    col->net = network_make(col, port);
    col->port = port;
    return col;
}

ColStatus
col_destroy(ColInstance *col)
{
    network_destroy(col->net);
    router_destroy(col->router);
    apr_pool_destroy(col->pool);

    return COL_OK;
}

ColStatus
col_install_file(ColInstance *col, const char *path)
{
    return COL_OK;
}

ColStatus
col_install_str(ColInstance *col, const char *str)
{
    ColParser *parser;
    AstProgram *program;

    parser = parser_make(col);
    program = parser_do_parse(parser, str);
    parser_destroy(parser);

    return COL_OK;
}

ColStatus
col_start(ColInstance *col)
{
    router_start(col->router);
    network_start(col->net);

    return COL_OK;
}

void
col_set_other(ColInstance *col, int target_port)
{
    col->target_port = target_port;
    snprintf(col->target_loc_spec, sizeof(col->target_loc_spec),
             "tcp:localhost:%d", target_port);
}

void
col_do_ping(ColInstance *col)
{
    router_enqueue(col->router, NULL);
}
