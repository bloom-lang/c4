#include <apr_general.h>

#include "c4-api.h"

int
main(void)
{
    C4Instance *c4;

    c4_initialize();

    c4 = c4_make(0);
    c4_destroy(c4);

    c4_terminate();
}
