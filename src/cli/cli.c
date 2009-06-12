#include <stdio.h>

#include "col-api.h"

int
main(void)
{
    ColInstance *c = col_init();
    col_destroy(c);
    printf("hello, world\n");
}
