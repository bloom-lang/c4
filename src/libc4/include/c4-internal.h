#ifndef C4_INTERNAL_H
#define C4_INTERNAL_H

/* Enable assertions? */
#if 0
#define ASSERT_ENABLED
#endif

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
    apr_pool_t *pool;
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
