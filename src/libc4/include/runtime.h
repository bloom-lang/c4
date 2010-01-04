#ifndef C4_RUNTIME_H
#define C4_RUNTIME_H

#include <apr_queue.h>
#include <apr_thread_proc.h>

apr_thread_t *c4_instance_start(int port, apr_queue_t *queue, apr_pool_t *pool);

#endif  /* C4_RUNTIME_H */
