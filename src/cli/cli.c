#include <stdio.h>

#include "types/tuple.h"

int
main(void) {
    Tuple *t = alloc_tuple();
    printf("hello, world: %d", t->foo);
}
