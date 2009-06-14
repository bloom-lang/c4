#ifndef ROUTER_H
#define ROUTER_H

#include <apr_thread_proc.h>

typedef struct ColRouter
{
    ColInstance *col;
    apr_pool_t *pool;

    /* Thread info for router thread */
    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;
} ColRouter;

ColRouter *router_make(ColInstance *col);
void router_destroy(ColRouter *router);

void router_start(ColRouter *router);

#endif  /* ROUTER_H */
