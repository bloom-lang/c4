#include <apr_file_io.h>

#include "c4-internal.h"
#include "net/network.h"
#include "router.h"
#include "storage/sqlite.h"
#include "types/catalog.h"

static apr_status_t c4_cleanup(void *data);

void
c4_initialize(void)
{
    apr_status_t s = apr_initialize();
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

void
c4_terminate(void)
{
    apr_terminate();
}

static Datum
get_local_addr(int port, apr_pool_t *tmp_pool)
{
    char buf[APRMAXHOSTLEN + 1];
    char addr[APRMAXHOSTLEN + 1 + 20];
    apr_status_t s;

    s = apr_gethostname(buf, sizeof(buf), tmp_pool);
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
get_c4_base_dir(int port, apr_pool_t *pool, apr_pool_t *tmp_pool)
{
    char *home_dir;
    char *base_dir;
    apr_status_t s;

    home_dir = get_user_home_dir(tmp_pool);
    base_dir = apr_psprintf(pool, "%s/c4_home/tcp_%d",
                            home_dir, port);
    s = apr_dir_make_recursive(base_dir, APR_FPROT_OS_DEFAULT, tmp_pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    printf("c4: Using base_dir = %s\n", base_dir);
    return base_dir;
}

C4Instance *
c4_make(int port)
{
    apr_status_t s;
    apr_pool_t *pool;
    C4Instance *c4;

    s = apr_pool_create(&pool, NULL);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    c4 = apr_pcalloc(pool, sizeof(*c4));
    c4->pool = pool;
    c4->tmp_pool = make_subpool(c4->pool);
    c4->log = logger_make(c4);
    c4->cat = cat_make(c4);
    c4->router = router_make(c4);
    c4->net = network_make(c4, port);
    c4->port = network_get_port(c4->net);
    c4->local_addr = get_local_addr(c4->port, c4->tmp_pool);
    c4->base_dir = get_c4_base_dir(c4->port, c4->pool, c4->tmp_pool);
    c4->sql = sqlite_init(c4);

    router_start(c4->router);
    network_start(c4->net);

    apr_pool_cleanup_register(pool, c4, c4_cleanup, apr_pool_cleanup_null);

    return c4;
}

static apr_status_t
c4_cleanup(void *data)
{
    C4Instance *c4 = (C4Instance *) data;

    network_destroy(c4->net);
    router_destroy(c4->router);

    return APR_SUCCESS;
}

C4Status
c4_destroy(C4Instance *c4)
{
    apr_pool_destroy(c4->pool);

    return C4_OK;
}

/*
 * Read the file at the specified filesystem path into memory, parse it, and
 * then install the resulting program into the specified C4 runtime. XXX:
 * We assume that the file is small enough that it can be slurped into a
 * single memory buffer without too much pain.
 */
C4Status
c4_install_file(C4Instance *c4, const char *path)
{
    apr_status_t s;
    apr_file_t *file;
    apr_pool_t *file_pool;
    apr_finfo_t finfo;
    char *buf;
    apr_size_t file_size;
    C4Status result;

    file_pool = make_subpool(c4->pool);
    s = apr_file_open(&file, path, APR_READ | APR_BUFFERED,
                      APR_OS_DEFAULT, file_pool);
    if (s != APR_SUCCESS)
    {
        result = C4_ERROR;
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
    result = c4_install_str(c4, buf);

done:
    apr_pool_destroy(file_pool);
    return result;
}

/*
 * Install the program contained in the specified string into the C4
 * runtime.
 *
 * XXX: Note that this is asynchronous; should we provide a convenient means
 * for the caller to wait until the program has been installed?
 */
C4Status
c4_install_str(C4Instance *c4, const char *str)
{
    router_enqueue_program(c4->router, str);

    return C4_OK;
}
