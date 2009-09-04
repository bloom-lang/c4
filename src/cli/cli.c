#include <apr_general.h>
#include <apr_getopt.h>
#include <apr_strings.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "col-api.h"

static void usage(void);
static void exec_file(apr_int16_t port, const char *srcfile);

int
main(int argc, const char *argv[])
{
    static const apr_getopt_option_t opt_option[] =
        {
            { "help", 'h', false, "show help" },
            { "port", 'p', true, "port number" },
            { NULL, 0, 0, NULL }
        };
    apr_pool_t *pool;
    apr_getopt_t *opt;
    int optch;
    const char *optarg;
    apr_status_t s;
    apr_int64_t port = 0;

    col_initialize();

    (void) apr_pool_create(&pool, NULL);
    (void) apr_getopt_init(&opt, pool, argc, argv);

    while ((s = apr_getopt_long(opt, opt_option,
                                &optch, &optarg)) == APR_SUCCESS)
    {
        switch (optch)
        {
            case 'h':
                usage();
                break;

            case 'p':
                port = apr_atoi64(optarg);
                if (port < 0 || port > APR_INT16_MAX)
                    usage();
                break;
        }
    }

    if (s != APR_EOF)
        usage();

    /*
     * opt->ind has the index of the last element of argv that was processed
     * by getopt. We expect there to be exactly one more argv element (input
     * source file).
     */
    if (opt->ind + 1 != argc)
        usage();

    exec_file((apr_int16_t) port, argv[opt->ind]);

    apr_pool_destroy(pool);
    col_terminate();
}

static void
usage(void)
{
    printf("Usage: coverlog [ -h | -p port ] srcfile\n");
    exit(1);
}

static void
exec_file(apr_int16_t port, const char *srcfile)
{
    ColInstance *c;
    ColStatus s;

    c = col_make(port);
    col_start(c);
    s = col_install_file(c, srcfile);
    if (s)
        printf("Failed to install file \"%s\": %d\n", srcfile, (int) s);
    else
        printf("Successfully installed file \"%s\"\n", srcfile);

    while (true)
        ;

    col_destroy(c);
}
