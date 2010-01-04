#ifndef C4_INTERNAL_H
#define C4_INTERNAL_H

#include "c4-api.h"

/* Commonly-used external headers */
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Commonly-used internal headers */
#include "types/datum.h"
#include "util/error.h"
#include "util/logger.h"
#include "util/mem.h"

struct C4Instance
{
    /*
     * The root APR pool for this instance of C4. This is created when
     * the instance is created, and destroyed after the instance has
     * shutdown.
     */
    apr_pool_t *pool;

    /*
     * Per-fixpoint APR pool for temporary allocations. This is
     * cleared at the end of each fixpoint. Given the need, we could
     * introduce more fine-grained temporary pools.
     */
    apr_pool_t *tmp_pool;

    struct C4Catalog *cat;
    struct C4Router *router;
    struct C4Network *net;
    C4Logger *log;
    int port;
    Datum local_addr;
    char *base_dir;
    struct SQLiteState *sql;
};

/* Utility macros */
#define Max(x, y)       ((x) > (y) ? (x) : (y))
#define Min(x, y)       ((x) < (y) ? (x) : (y))
#define Abs(x)          ((x) >= 0 ? (x) : -(x))

#endif
