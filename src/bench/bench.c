#include <apr_general.h>
#include <apr_getopt.h>

#include <stdbool.h>
#include <stdio.h>

#include "c4-api.h"

static void usage(void);

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

    c4_terminate();
    return 0;
}

static void
usage(void)
{
    ;
}

