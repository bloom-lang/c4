#include <apr_file_io.h>

#include "col-internal.h"
#include "net/network.h"
#include "router.h"
#include "types/catalog.h"

static apr_status_t col_cleanup(void *data);

void
col_initialize(void)
{
    apr_status_t s = apr_initialize();
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

void
col_terminate(void)
{
    apr_terminate();
}

static Datum
get_local_addr(int port, apr_pool_t *pool)
{
    char buf[APRMAXHOSTLEN + 1];
    char addr[APRMAXHOSTLEN + 1 + 20];
    apr_status_t s;

    /* XXX: use temporary pool? No leak with current APR implementation. */
    s = apr_gethostname(buf, sizeof(buf), pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    snprintf(addr, sizeof(addr), "tcp:%s:%d", buf, port);
    printf("Local address = %s\n", addr);
    return string_from_str(addr);
}

static char *
get_user_home_dir(apr_pool_t *pool)
{
    apr_status_t s;
    apr_uid_t uid;
    apr_gid_t gid;
    char *user_name;
    char *home_dir;

    s = apr_uid_current(&uid, &gid, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_uid_name_get(&user_name, uid, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_uid_homepath_get(&home_dir, user_name, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return home_dir;
}

static char *
get_col_base_dir(int port, apr_pool_t *pool)
{
    char *home_dir;
    char *base_dir;
    apr_status_t s;

    home_dir = get_user_home_dir(pool);
    base_dir = apr_psprintf(pool, "%s/col_home/tcp_%d",
                            home_dir, port);
    s = apr_dir_make_recursive(base_dir, APR_FPROT_OS_DEFAULT, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    printf("col: Using base_dir = %s\n", base_dir);
    return base_dir;
}

ColInstance *
col_make(int port)
{
    apr_status_t s;
    apr_pool_t *pool;
    ColInstance *col;
    char *db_fname;
    int res;

    s = apr_pool_create(&pool, NULL);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    col = apr_pcalloc(pool, sizeof(*col));
    col->pool = pool;
    col->log = logger_make(col);
    col->cat = cat_make(col);
    col->router = router_make(col);
    col->net = network_make(col, port);
    col->port = network_get_port(col->net);
    col->local_addr = get_local_addr(col->port, col->pool);
    col->base_dir = get_col_base_dir(col->port, col->pool);
    col->xact_in_progress = false;

    db_fname = apr_pstrcat(pool, col->base_dir, "/", "sqlite.db");
    if ((res = sqlite3_open(db_fname, &col->sql_db)) != 0)
        ERROR("sqlite3_open failed: %d", res);

    apr_pool_cleanup_register(pool, col, col_cleanup, apr_pool_cleanup_null);

    return col;
}

static apr_status_t
col_cleanup(void *data)
{
    ColInstance *col = (ColInstance *) data;

    network_destroy(col->net);
    router_destroy(col->router);
    (void) sqlite3_close(col->sql_db);

    return APR_SUCCESS;
}

ColStatus
col_destroy(ColInstance *col)
{
    apr_pool_destroy(col->pool);

    return COL_OK;
}

/*
 * Read the file at the specified filesystem path into memory, parse it, and
 * then install the resulting program into the specified COL runtime. XXX:
 * We assume that the file is small enough that it can be slurped into a
 * single memory buffer without too much pain.
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
    s = apr_file_open(&file, path, APR_READ | APR_BUFFERED,
                      APR_OS_DEFAULT, file_pool);
    if (s != APR_SUCCESS)
    {
        result = COL_ERROR;
        goto done;
    }

    /*
     * Get the file size, and allocate an appropriately-sized buffer to hold
     * the file contents. XXX: There is a trivial race condition here.
     */
    s = apr_file_info_get(&finfo, APR_FINFO_SIZE, file);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    file_size = (apr_size_t) finfo.size;
    buf = apr_palloc(file_pool, file_size + 1);

    s = apr_file_read(file, buf, &file_size);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
    if (file_size != (apr_size_t) finfo.size)
        FAIL();

    buf[file_size] = '\0';
    result = col_install_str(col, buf);

done:
    apr_pool_destroy(file_pool);
    return result;
}

/*
 * Install the program contained in the specified string into the COL
 * runtime.
 *
 * XXX: Note that this is asynchronous; should we provide a convenient means
 * for the caller to wait until the program has been installed?
 */
ColStatus
col_install_str(ColInstance *col, const char *str)
{
    router_enqueue_program(col->router, str);

    return COL_OK;
}

ColStatus
col_start(ColInstance *col)
{
    router_start(col->router);
    network_start(col->net);

    return COL_OK;
}
