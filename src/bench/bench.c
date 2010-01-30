#include <apr_general.h>
#include <apr_getopt.h>
#include <apr_strings.h>
#include <apr_thread_cond.h>
#include <apr_thread_mutex.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "c4-api.h"
#include "util/thread_sync.h"

static void usage(void);
static void do_net_bench(apr_pool_t *pool);
static void do_perf_bench(apr_pool_t *pool);

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

static void
done_table_cb(struct Tuple *tuple, struct TableDef *tbl_def, void *data)
{
    C4ThreadSync *sync = (C4ThreadSync *) data;

    printf("%s invoked!\n", __func__);
    thread_sync_signal(sync);
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
do_net_bench(apr_pool_t *pool)
{
    C4Client *c1;
    C4Client *c2;
    C4ThreadSync *sync;
    char *ping_fact;

    c1 = c4_make(0);
    c2 = c4_make(0);

    net_install_program(c1);
    net_install_program(c2);

    sync = thread_sync_make(pool);
    thread_sync_prepare(sync);
    c4_register_callback(c1, "done", done_table_cb, sync);

    ping_fact = apr_psprintf(pool, "ping(\"tcp:localhost:%d\", \"tcp:localhost:%d\", 0);",
                             c4_get_port(c1), c4_get_port(c2));

    c4_install_str(c1, ping_fact);
    thread_sync_wait(sync);

    c4_destroy(c1);
    c4_destroy(c2);
}

static void
perf_install_program(C4Client *c)
{
    c4_install_str(c, "define(t, keys(0), {int8, int8});");
    c4_install_str(c, "t(A, B + 1) :- t(A, B), B < 10000000;");
}

static void
do_perf_bench(apr_pool_t *pool)
{
    C4Client *c;

    c = c4_make(0);
    perf_install_program(c);
    c4_install_str(c, "t(0, 0);");

    c4_destroy(c);
}
