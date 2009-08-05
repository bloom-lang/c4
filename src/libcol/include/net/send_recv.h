#ifndef SEND_RECV_H
#define SEND_RECV_H

#include <apr_network_io.h>

typedef struct RecvThread RecvThread;
typedef struct SendThread SendThread;

RecvThread *recv_thread_make(ColInstance *col, apr_socket_t *sock,
                             apr_pool_t *pool);
void recv_thread_start(RecvThread *rt);
const char *recv_thread_get_loc(RecvThread *rt);

SendThread *send_thread_make(ColInstance *col, const char *remote_loc,
                             apr_pool_t *pool);
void send_thread_start(SendThread *st);
void send_thread_enqueue(SendThread *st, const char *tbl_name, Tuple *tuple);
const char *send_thread_get_loc(SendThread *st);

#endif  /* SEND_RECV_H */
