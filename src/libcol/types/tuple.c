#include <stdlib.h>

#include "col-internal.h"
#include "types/tuple.h"

Tuple *
tuple_make()
{
    Tuple *t = (Tuple *) malloc(sizeof(Tuple));
    return t;
}
