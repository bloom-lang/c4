#ifndef COL_INTERNAL_H
#define COL_INTERNAL_H

#include "col-api.h"

/* Commonly-used external headers */
#include <apr_general.h>
#include <apr_pools.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Commonly-used internal headers */
#include "util/error.h"
#include "util/mem.h"

struct ColInstance
{
    apr_pool_t *pool;
    struct ColRouter *router;
    struct ColNetwork *net;
    int port;
    char target_loc_spec[128];
    int target_port;
};

#endif
