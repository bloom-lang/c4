#include <stdlib.h>
#include <stdio.h>

#include "col-api.h"

static void usage();
static void exec_file(const char *srcfile);

int
main(int argc, char **argv)
{
    if (argc < 1 || argc > 2)
        usage();

    exec_file(argv[1]);
    printf("hello, world\n");
}

static void
usage()
{
    printf("Usage: coverlog srcfile\n");
    exit(1);
}

static void
exec_file(const char *srcfile)
{
    ColInstance *c;
    ColStatus s;

    col_initialize();
    c = col_make();
#if 0
    s = col_install_file(c, srcfile);
    if (s)
        printf("Failed to install file: %d", (int) s);
#endif
    s = col_install_str(c, "program bar; foo(X) :- baz(X);");

    col_destroy(c);
    col_terminate();
}
