#include <apr_file_io.h>
#include <apr_thread_proc.h>

#include "c4-api.h"
#include "c4-internal.h"
#include "router.h"
#include "runtime.h"
#include "util/thread_sync.h"

/*
 * Client state. This is manipulated by C4 client APIs; they mostly insert
 * elements into the thread-safe queue, which is consumed by the C4
 * runtime. Note that it is NOT safe for the runtime to allocate resources
 * (other than sub-pools) from the client state's pool.
 */
struct C4Client
{
    apr_pool_t *pool;
    /* Reset after each API command */
    apr_pool_t *tmp_pool;
    /* Runtime state */
    C4Runtime *runtime;
    apr_thread_t *runtime_thread;
    /* State for passing messages => runtime */
    WorkItem *wi;
    C4ThreadSync *thread_sync;
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
    client->tmp_pool = make_subpool(client->pool);
    client->thread_sync = thread_sync_make(client->pool);
    client->runtime = c4_runtime_start(port, client->thread_sync, client->pool,
                                       &client->runtime_thread);
    client->wi = apr_palloc(client->pool, sizeof(WorkItem));
    client->wi->sync = client->thread_sync;

    return client;
}

C4Status
c4_destroy(C4Client *client)
{
    WorkItem *wi = client->wi;
    apr_status_t s;
    apr_status_t thread_status;

    wi->kind = WI_SHUTDOWN;
    runtime_enqueue_work(client->runtime, wi);

    s = apr_thread_join(&thread_status, client->runtime_thread);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
    if (thread_status != APR_SUCCESS)
        FAIL_APR(thread_status);

    apr_pool_destroy(client->pool);
    return C4_OK;
}

int
c4_get_port(C4Client *client)
{
    return client->runtime->port;
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
    apr_finfo_t finfo;
    char *buf;
    apr_size_t file_size;
    C4Status result;

    s = apr_file_open(&file, path, APR_READ | APR_BUFFERED,
                      APR_OS_DEFAULT, client->tmp_pool);
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
    buf = apr_palloc(client->tmp_pool, file_size + 1);

    s = apr_file_read(file, buf, &file_size);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
    if (file_size != (apr_size_t) finfo.size)
        FAIL();

    buf[file_size] = '\0';
    result = c4_install_str(client, buf);

done:
    apr_pool_clear(client->tmp_pool);
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
    WorkItem *wi = client->wi;

    wi->kind = WI_PROGRAM;
    wi->program_src = str;
    runtime_enqueue_work(client->runtime, wi);

    return C4_OK;
}

char *
c4_dump_table(C4Client *client, const char *tbl_name)
{
    WorkItem *wi = client->wi;

    wi->kind = WI_DUMP_TABLE;
    wi->tbl_name = tbl_name;
    wi->buf = sbuf_make(client->pool);
    runtime_enqueue_work(client->runtime, wi);

    return wi->buf->data;
}

C4Status
c4_register_callback(C4Client *client, const char *tbl_name,
                     C4TupleCallback callback, void *data)
{
    WorkItem *wi = client->wi;

    wi->kind = WI_CALLBACK;
    wi->callback_tbl_name = tbl_name;
    wi->callback = callback;
    wi->callback_data = data;
    runtime_enqueue_work(client->runtime, wi);

    return C4_OK;
}
