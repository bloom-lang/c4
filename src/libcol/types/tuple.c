#include <stdlib.h>

#include "types/tuple.h"

Tuple *
alloc_tuple() {
    Tuple *t = (Tuple *) malloc(sizeof(Tuple));
    return t;
}
