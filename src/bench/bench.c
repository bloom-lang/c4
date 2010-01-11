#include <apr_general.h>
#include <apr_getopt.h>
#include <apr_strings.h>
#include <apr_thread_cond.h>
#include <apr_thread_mutex.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "c4-api.h"

static void usage(void);
static void do_net_bench(apr_pool_t *pool);
static void do_perf_bench(apr_pool_t *pool);
static void net_done_cb(struct Tuple *tuple, struct TableDef *tbl_def,
                        void *data);
static void net_install_program(C4Client *c);

int
main(int argc, const char *argv[])
{
    static const apr_getopt_option_t opt_option[] =
        {
            {"net", 'n', false, "network benchmark"},
            { NULL, 0, 0, NULL }
        };
    apr_pool_t *pool;
    apr_getopt_t *opt;
    int optch;
    const char *optarg;
    apr_status_t s;
    bool net_bench = false;

    c4_initialize();

    (void) apr_pool_create(&pool, NULL);
    (void) apr_getopt_init(&opt, pool, argc, argv);

    while ((s = apr_getopt_long(opt, opt_option,
                                &optch, &optarg)) == APR_SUCCESS)
    {
        switch (optch)
        {
            case 'n':
                net_bench = true;
                break;

            default:
                printf("Unrecognized option: %c\n", optch);
                usage();
        }
    }

    if (s != APR_EOF)
        usage();

    if (net_bench)
        do_net_bench(pool);
    else
        do_perf_bench(pool);

    apr_pool_destroy(pool);
    c4_terminate();
    return 0;
}

static void
usage(void)
{
    printf("Usage: bench [ -n ]\n");
    exit(1);
}

typedef struct NetCallbackData
{
    bool is_done;
    apr_thread_mutex_t *lock;
    apr_thread_cond_t *cond;
} NetCallbackData;

static void
do_net_bench(apr_pool_t *pool)
{
    C4Client *c1;
    C4Client *c2;
    NetCallbackData cb_data;
    char *ping_fact;

    c1 = c4_make(0);
    c2 = c4_make(0);

    net_install_program(c1);
    net_install_program(c2);

    cb_data.is_done = false;
    (void) apr_thread_mutex_create(&cb_data.lock, APR_THREAD_MUTEX_DEFAULT, pool);
    (void) apr_thread_cond_create(&cb_data.cond, pool);
    c4_register_callback(c1, "done", net_done_cb, &cb_data);

    ping_fact = apr_psprintf(pool, "ping(\"tcp:localhost:%d\", \"tcp:localhost:%d\", 0);",
                             c4_get_port(c1), c4_get_port(c2));

    c4_install_str(c1, ping_fact);

    do {
        apr_thread_cond_wait(cb_data.cond, cb_data.lock);
    } while (!cb_data.is_done);

    apr_thread_mutex_unlock(cb_data.lock);
    c4_destroy(c1);
    c4_destroy(c2);
}

static void
net_done_cb(struct Tuple *tuple, struct TableDef *tbl_def, void *data)
{
    NetCallbackData *cb_data = (NetCallbackData *) data;

    printf("%s invoked!\n", __func__);

    apr_thread_mutex_lock(cb_data->lock);
    cb_data->is_done = true;
    apr_thread_cond_signal(cb_data->cond);
    apr_thread_mutex_unlock(cb_data->lock);
}

static void
net_install_program(C4Client *c)
{
    c4_install_str(c, "define(ping, keys(0), {@string, string, int8});");
    c4_install_str(c, "define(done, keys(0), {int8});");
    c4_install_str(c, "ping(X, Y, C + 1) :- ping(Y, X, C), C < 100000;");
    c4_install_str(c, "done(C) :- ping(_, _, C), C >= 100000;");
}

static void
do_perf_bench(apr_pool_t *pool)
{
    ;
}
