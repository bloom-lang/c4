#ifndef COL_INTERNAL_H
#define COL_INTERNAL_H

#include "col-api.h"

/* Commonly-used external headers */
#include <apr_pools.h>
#include <stdbool.h>
#include <stdio.h>

/* Commonly-used internal headers */
#include "util/error.h"
#include "util/mem.h"

struct ColInstance
{
    apr_pool_t *pool;
    struct ColRouter *router;
    struct ColNetwork *net;
};

#endif
