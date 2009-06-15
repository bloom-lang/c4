#ifndef CONNECTION_H
#define CONNECTION_H

#include "net/network.h"
#include "types/tuple.h"

typedef struct ColConnection ColConnection;

typedef enum ConnectionState
{
    COL_NEW,
    COL_CONNECTING,
    COL_CONNECTED
} ConnectionState;

ColConnection *connection_make(apr_socket_t *sock, ColInstance *col, apr_pool_t *pool);
ColConnection *connection_new_connect(const char *remote_loc, ColInstance *col,
                                      apr_pool_t *net_pool);
void connection_destroy(ColConnection *conn);

void connection_send(ColConnection *conn, Tuple *tuple);

ConnectionState connection_get_state(ColConnection *conn);
char *connection_get_remote_loc(ColConnection *conn);

#endif  /* CONNECTION_H */
