/* XXX: __MACH__ is probably slightly wrong */
#if defined(__linux__) || defined(__FreeBSD__) || defined(__MACH__)
#define HAVE_EXECINFO
#include <execinfo.h>
#endif

#include "col-internal.h"

static void print_backtrace(void);

void
simple_error(const char *file, int line_num)
{
    fprintf(stderr, "FATAL ERROR at %s:%d\n", file, line_num);
    print_backtrace();
    fflush(stderr);
    abort();
}

void
var_error(const char *file, int line_num, const char *fmt, ...)
{
    ;
}

int
assert_fail(const char *cond, const char *file, int line_num)
{
    fprintf(stderr, "ASSERT FAILED: \"%s\", at %s:%d\n",
            cond, file, line_num);
    print_backtrace();
    fflush(stderr);
    abort();
    return 0;   /* Keep the compiler quiet */
}

static void
print_backtrace(void)
{
#ifdef HAVE_EXECINFO
    void *bt_array[128];
    char **bt_strings;
    apr_size_t nframes;
    apr_size_t i;

    nframes = backtrace(bt_array, 128);
    bt_strings = backtrace_symbols(bt_array, nframes);

    fprintf(stderr, "STACK TRACE: (%zd frames)\n", nframes);
    for (i = 0; i < nframes; i++)
        fprintf(stderr, "    %s\n", bt_strings[i]);

    free(bt_strings);
#endif
}
