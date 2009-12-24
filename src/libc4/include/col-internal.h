#ifndef COL_INTERNAL_H
#define COL_INTERNAL_H

/* Enable assertions? */
#if 0
#define ASSERT_ENABLED
#endif

#include "col-api.h"

/* Commonly-used external headers */
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Commonly-used internal headers */
#include "types/datum.h"
#include "util/error.h"
#include "util/logger.h"
#include "util/mem.h"

struct ColInstance
{
    apr_pool_t *pool;
    struct ColCatalog *cat;
    struct ColRouter *router;
    struct ColNetwork *net;
    ColLogger *log;
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
