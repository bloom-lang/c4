#ifndef C4_RUNTIME_H
#define C4_RUNTIME_H

#include <apr_thread_proc.h>

C4Runtime *c4_runtime_start(int port, apr_pool_t *pool, apr_thread_t **thread);

#endif  /* C4_RUNTIME_H */
