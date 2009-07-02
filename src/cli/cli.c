#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "col-api.h"

static void usage(void);
static void exec_file(const char *srcfile);

int
main(int argc, char **argv)
{
    if (argc <= 1 || argc > 2)
        usage();

    col_initialize();
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
    s = col_install_file(c, srcfile);
    if (s)
        printf("Failed to install file: %d\n", (int) s);

    while (true)
        ;

    col_destroy(c);
}
