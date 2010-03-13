#include <apr_general.h>
#include <apr_getopt.h>
#include <apr_strings.h>
#include <apr_time.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "c4-api.h"
#include "util/thread_sync.h"

typedef void (*program_install_f)(C4Client *c);

static void
usage(void)
{
    printf("Usage: bench [ -a | -n | -j ]\n");
    exit(1);
}

static void
done_table_cb(struct Tuple *tuple, struct TableDef *tbl_def,
              bool is_delete, void *data)
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

    c1 = c4_make(pool, 0);
    c2 = c4_make(pool, 0);

    net_install_program(c1);
    net_install_program(c2);

    sync = thread_sync_make(pool);
    c4_register_callback(c1, "done", done_table_cb, sync);

    ping_fact = apr_psprintf(pool, "ping(\"tcp:localhost:%d\", \"tcp:localhost:%d\", 0);",
                             c4_get_port(c1), c4_get_port(c2));

    c4_install_str(c1, ping_fact);
    thread_sync_wait(sync);
}

static void
agg_install_program(C4Client *c)
{
    c4_install_str(c, "define(t, keys(0), {int8});");
    c4_install_str(c, "define(b, keys(0), {int8, int8});");
    c4_install_str(c, "define(c, keys(0), {int8, int8});");
    c4_install_str(c, "b(X, Y + 1) :- b(X, Y), Y < 150000;");
    c4_install_str(c, "b(X, 0) :- t(X);");
    c4_install_str(c, "t(X + 1) :- t(X), X < 30;");
    c4_install_str(c, "c(X, count<Y>) :- b(X, Y);");
}

static void
perf_install_program(C4Client *c)
{
    c4_install_str(c, "define(t, keys(0), {int8});");
    c4_install_str(c, "define(s, keys(0), {int8});");
    c4_install_str(c, "t(A + 1) :- t(A), A < 3000000;");
    c4_install_str(c, "s(A) :- t(A);");
}

static void
join_install_program(C4Client *c)
{
    c4_install_str(c, "define(t, keys(0), {int8});");
    c4_install_str(c, "define(s, keys(0), {int8});");
    c4_install_str(c, "s(0);");
    c4_install_str(c, "t(A + 1) :- t(A), s(B), A >= B, A < 3000000;");
}

static void
do_simple_bench(program_install_f prog, apr_pool_t *pool)
{
    C4Client *c;

    c = c4_make(pool, 0);
    (*prog)(c);
    c4_install_str(c, "t(0);");
}

int
main(int argc, const char *argv[])
{
    static const apr_getopt_option_t opt_option[] =
        {
            {"agg", 'a', false, "agg benchmark"},
            {"join", 'j', false, "join benchmark"},
            {"net", 'n', false, "network benchmark"},
            { NULL, 0, 0, NULL }
        };
    apr_pool_t *pool;
    apr_getopt_t *opt;
    int optch;
    const char *optarg;
    apr_status_t s;
    bool agg_bench = false;
    bool join_bench = false;
    bool net_bench = false;
    apr_time_t start_time;

    c4_initialize();

    (void) apr_pool_create(&pool, NULL);
    (void) apr_getopt_init(&opt, pool, argc, argv);

    while ((s = apr_getopt_long(opt, opt_option,
                                &optch, &optarg)) == APR_SUCCESS)
    {
        switch (optch)
        {
            case 'a':
                agg_bench = true;
                break;

            case 'j':
                join_bench = true;
                break;

            case 'n':
                net_bench = true;
                break;

            default:
                printf("Unrecognized option: %c\n", optch);
                usage();
        }
    }

    if (s != APR_EOF || (join_bench && net_bench))
        usage();

    start_time = apr_time_now();

    if (agg_bench)
        do_simple_bench(agg_install_program, pool);
    else if (join_bench)
        do_simple_bench(join_install_program, pool);
    else if (net_bench)
        do_net_bench(pool);
    else
        do_simple_bench(perf_install_program, pool);

    printf("Benchmark duration: %" APR_TIME_T_FMT " usec\n",
           (apr_time_now() - start_time));

    start_time = apr_time_now();
    apr_pool_destroy(pool);
    printf("Shutdown duration: %" APR_TIME_T_FMT " usec\n",
           (apr_time_now() - start_time));

    c4_terminate();
    return 0;
}
