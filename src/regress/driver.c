#include <apr_general.h>

#include "c4-api.h"

#define INPUT_FILE_DIR "/Users/neilconway/build_c4/src/regress/input"
#define EXPECTED_FILE_DIR "/Users/neilconway/build_c4/src/regress/expected"

static char *test_files[] = {
    "hello, world",
    "bar",
    "baz"
};

int
main(void)
{
    C4Instance *c4;

    c4_initialize();

    c4 = c4_make(0);
    c4_start(c4);
    c4_destroy(c4);

    c4_terminate();
}
