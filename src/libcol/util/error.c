#include "col-internal.h"

#include <stdlib.h>

void
fatal_error(const char *file, int line_num)
{
    fprintf(stderr, "FATAL ERROR at %s:%d\n", file, line_num);
    fflush(stderr);
    abort();
}

int
assert_fail(const char *cond, const char *file, int line_num)
{
    fprintf(stderr, "ASSERT FAILED: \"%s\", at %s:%d\n",
            cond, file, line_num);
    fflush(stderr);
    abort();
    return 0;   /* Keep the compiler quiet */
}

