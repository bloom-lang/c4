#ifndef C4_RUNTIME_H
#define C4_RUNTIME_H

#include <apr_thread_proc.h>

#include "c4-api-callback.h"
#include "types/catalog.h"
#include "types/tuple.h"
#include "util/strbuf.h"
#include "util/thread_sync.h"

C4Runtime *c4_runtime_start(int port, C4ThreadSync *thread_sync,
                            apr_pool_t *pool, apr_thread_t **thread);

typedef enum WorkItemKind
{
    WI_TUPLE,
    WI_PROGRAM,
    WI_DUMP_TABLE,
    WI_CALLBACK,
    WI_SHUTDOWN
} WorkItemKind;

typedef struct WorkItem
{
    WorkItemKind kind;
    C4ThreadSync *sync;

    /* WI_TUPLE: */
    Tuple *tuple;
    TableDef *tbl_def;

    /* WI_PROGRAM: */
    const char *program_src;

    /* WI_DUMP_TABLE: */
    const char *tbl_name;
    StrBuf *buf;

    /* WI_CALLBACK */
    const char *callback_tbl_name;
    C4TupleCallback callback;
    void *callback_data;
} WorkItem;

void runtime_enqueue_work(C4Runtime *c4, WorkItem *wi);

#endif  /* C4_RUNTIME_H */
