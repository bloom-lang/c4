#ifndef C4_INTERNAL_H
#define C4_INTERNAL_H

/* Commonly-used external headers */
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct C4Runtime C4Runtime;

/* Commonly-used internal headers */
#include "types/datum.h"
#include "util/error.h"
#include "util/logger.h"
#include "util/mem.h"

/*
 * Private runtime state. This is not visible to the client, and can be safely
 * modified by the router thread.
 */
struct C4Runtime
{
    /*
     * The root APR pool for this instance of C4. This is created when the
     * instance is created, and destroyed when the instance is shutdown.
     */
    apr_pool_t *pool;

    /*
     * APR pool for temporary allocations. This is cleared at the end of each
     * fixpoint.
     */
    apr_pool_t *tmp_pool;

    /* Various C4 subsystems */
    C4Logger *log;
    struct C4Catalog *cat;
    struct C4Network *net;
    struct C4Router *router;
    struct SQLiteState *sql;
    struct C4Timer *timer;
    struct TuplePoolMgr *tpool_mgr;

    int port;
    Datum local_addr;
    char *base_dir;
};

/* Utility macros */
#define Max(x, y)       ((x) > (y) ? (x) : (y))
#define Min(x, y)       ((x) < (y) ? (x) : (y))
#define Abs(x)          ((x) >= 0 ? (x) : -(x))

/* Mark that a function argument is intentionally unused */
#ifndef __unused
#define __unused __attribute__ ((__unused__))
#endif

#endif
