#include <apr_general.h>
#include <apr_getopt.h>
#include <apr_strings.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "c4-api.h"

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
    char *src_string = NULL;
    C4Client *c;

    c4_initialize();

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

            case 's':
                src_string = apr_pstrdup(pool, optarg);
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
    if (src_string != NULL)
        c4_install_str(c, src_string);

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
