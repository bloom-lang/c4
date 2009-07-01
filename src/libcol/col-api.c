#include <apr_file_io.h>

#include "col-internal.h"
#include "net/network.h"
#include "parser/analyze.h"
#include "parser/parser.h"
#include "router.h"
#include "types/catalog.h"

void
col_initialize(void)
{
    apr_status_t s = apr_initialize();
    if (s != APR_SUCCESS)
        FAIL();
}

void
col_terminate(void)
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
    col->cat = cat_make(col);
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

/*
 * Read the file at the specified location into memory, parse it, and then
 * install the resulting program into the specified COL runtime. XXX: We
 * currently assume that the file is small enough that it can be slurped
 * into a single memory buffer without too much pain.
 */
ColStatus
col_install_file(ColInstance *col, const char *path)
{
    apr_status_t s;
    apr_file_t *file;
    apr_pool_t *file_pool;
    apr_finfo_t finfo;
    char *buf;
    apr_size_t file_size;
    ColStatus result;

    file_pool = make_subpool(col->pool);
    s = apr_file_open(&file, path, APR_READ|APR_BUFFERED,
                      APR_OS_DEFAULT, file_pool);
    if (s != APR_SUCCESS)
    {
        result = COL_ERROR;
        goto done;
    }

    /*
     * Get the file size, and allocate an appropriately-sized buffer to hold
     * the file contents. There is a trivial race condition here.
     */
    s = apr_file_info_get(&finfo, APR_FINFO_SIZE, file);
    if (s != APR_SUCCESS)
        FAIL();

    file_size = (apr_size_t) finfo.size;
    buf = apr_palloc(file_pool, file_size + 1);

    s = apr_file_read(file, buf, &file_size);
    if (s != APR_SUCCESS)
        FAIL();
    if (file_size != (apr_size_t) finfo.size)
        FAIL();

    buf[file_size] = '\0';
    result = col_install_str(col, buf);

done:
    apr_pool_destroy(file_pool);
    return result;
}

ColStatus
col_install_str(ColInstance *col, const char *str)
{
    AstProgram *program;
    apr_pool_t *plan_pool;

    plan_pool = make_subpool(col->pool);
    program = parse_str(col, str, plan_pool);
    apr_pool_destroy(plan_pool);

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
    router_enqueue_tuple(col->router, NULL);
}
