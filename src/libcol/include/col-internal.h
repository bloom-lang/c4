#ifndef COL_INTERNAL_H
#define COL_INTERNAL_H

#include "col-api.h"

/* Commonly-used standard headers */
#include <stdbool.h>

/* Commonly-used internal headers */
#include "util/mem.h"

struct ColInstance
{
    struct ColNetwork *net;
};

#endif
