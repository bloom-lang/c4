#include <apr_file_io.h>
#include <apr_thread_proc.h>

#include "c4-api.h"
#include "c4-internal.h"
#include "router.h"
#include "runtime.h"

/*
 * Client state. This is manipulated by C4 client APIs; they mostly insert
 * elements into the thread-safe queue, which is consumed by the C4
 * runtime. Note that it is NOT safe for the runtime to allocate resources
 * (other than sub-pools) from the client state's pool.
 */
struct C4Client
{
    apr_pool_t *pool;
    C4Runtime *runtime;
    apr_thread_t *runtime_thread;
};

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

C4Client *
c4_make(int port)
{
    apr_pool_t *pool;
    C4Client *client;

    pool = make_subpool(NULL);
    client = apr_pcalloc(pool, sizeof(*client));
    client->pool = pool;
    client->runtime = c4_runtime_start(port, client->pool,
                                       &client->runtime_thread);
    return client;
}

C4Status
c4_destroy(C4Client *client)
{
    apr_status_t s;
    apr_status_t thread_status;

    runtime_enqueue_shutdown(client->runtime);

    s = apr_thread_join(&thread_status, client->runtime_thread);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
    if (thread_status != APR_SUCCESS)
        FAIL_APR(thread_status);

    apr_pool_destroy(client->pool);
    return C4_OK;
}

/*
 * Read the file at the specified filesystem path into memory, parse it, and
 * then install the resulting program into the specified C4 runtime. XXX:
 * We assume that the file is small enough that it can be slurped into a
 * single memory buffer without too much pain.
 */
C4Status
c4_install_file(C4Client *client, const char *path)
{
    apr_status_t s;
    apr_file_t *file;
    apr_pool_t *file_pool;
    apr_finfo_t finfo;
    char *buf;
    apr_size_t file_size;
    C4Status result;

    file_pool = make_subpool(client->pool);
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
    result = c4_install_str(client, buf);

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
c4_install_str(C4Client *client, const char *str)
{
    runtime_enqueue_program(client->runtime, str);

    return C4_OK;
}

char *
c4_dump_table(C4Client *client, const char *tbl_name)
{
    return runtime_enqueue_dump_table(client->runtime, tbl_name, client->pool);
}

C4Status
c4_register_callback(C4Client *client, const char *tbl_name,
                     C4TableCallback callback, void *data)
{
    runtime_enqueue_callback(client->runtime, tbl_name, callback, data);

    return C4_OK;
}
