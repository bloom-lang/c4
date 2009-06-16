#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "col-api.h"

static void usage(void);
static void exec_file(const char *srcfile);
static void ping_test(void);

int
main(int argc, char **argv)
{
    if (argc <= 1 || argc > 2)
        usage();

    col_initialize();

    if (strcmp(argv[1], "t") == 0)
        ping_test();
    else
        exec_file(argv[1]);

    col_terminate();
}

static void
usage(void)
{
    printf("Usage: coverlog srcfile\n");
    exit(1);
}

static void
exec_file(const char *srcfile)
{
    ColInstance *c;
    ColStatus s;

    c = col_make(0);
    col_start(c);
#if 0
    s = col_install_file(c, srcfile);
    if (s)
        printf("Failed to install file: %d", (int) s);
#endif
    s = col_install_str(c, "program bar; foo(X) :- baz(X);");

    while (1)
        ;

    col_destroy(c);
}

static void
ping_test(void)
{
    ColInstance *c1;
    ColInstance *c2;

    c1 = col_make(10000);
    col_start(c1);

    c2 = col_make(10001);
    col_start(c2);

    col_set_other(c1, 10001);
    col_set_other(c2, 10000);
    col_do_ping(c1);

    while (true)
        ;
}
