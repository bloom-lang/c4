#include <apr_general.h>
#include <apr_getopt.h>
#include <apr_strings.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "c4-api.h"

#define MAX_SOURCE_STRINGS 32

static void usage(void);
static C4Client *setup_c4(apr_pool_t *pool, apr_int16_t port,
                          const char *srcfile);

int
main(int argc, const char *argv[])
{
    static const apr_getopt_option_t opt_option[] =
        {
            { "src-string", 's', true, "install source" },
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
    char **src_strings;
    int num_strings;
    int i;
    C4Client *c;

    c4_initialize();

    (void) apr_pool_create(&pool, NULL);
    (void) apr_getopt_init(&opt, pool, argc, argv);

    num_strings = 0;
    src_strings = apr_palloc(pool, sizeof(*src_strings) * MAX_SOURCE_STRINGS);

    while ((s = apr_getopt_long(opt, opt_option,
                                &optch, &optarg)) == APR_SUCCESS)
    {
        switch (optch)
        {
            case 'h':
                usage();
                break;

            case 'p':
                if (port != 0)  /* Only allow a single "-p" option */
                    usage();
                port = apr_atoi64(optarg);
                if (port < 0 || port > APR_INT16_MAX)
                    usage();
                break;

            case 's':
                if (num_strings + 1 == MAX_SOURCE_STRINGS)
                    usage();
                src_strings[num_strings] = apr_pstrdup(pool, optarg);
                num_strings++;
                break;

            default:
                printf("Unrecognized option: %c\n", optch);
                usage();
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

    c = setup_c4(pool, (apr_int16_t) port, argv[opt->ind]);

    for (i = 0; i < num_strings; i++)
        c4_install_str(c, src_strings[i]);

    while (true)
        sleep(1);

    apr_pool_destroy(pool);
    c4_terminate();
    return 0;
}

static void
usage(void)
{
    printf("Usage: c4i [ -h | -p port | -s srctext ] srcfile\n");
    exit(1);
}

static C4Client *
setup_c4(apr_pool_t *pool, apr_int16_t port, const char *srcfile)
{
    C4Client *c;
    C4Status s;

    c = c4_make(pool, port);
    s = c4_install_file(c, srcfile);
    if (s)
        printf("Failed to install file \"%s\": %d\n", srcfile, (int) s);
    else
        printf("Successfully installed file \"%s\"\n", srcfile);

    return c;
}
