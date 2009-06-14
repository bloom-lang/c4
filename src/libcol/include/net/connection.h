#ifndef CONNECTION_H
#define CONNECTION_H

#include "net/network.h"
#include "types/tuple.h"

typedef struct ColConnection ColConnection;

typedef enum ConnectionState
{
    COL_CONNECTING,
    COL_CONNECTED
} ConnectionState;

ColConnection *connection_make(apr_socket_t *sock, const char *remote_loc,
                               ColNetwork *net, ColInstance *col);
ConnectionState connection_get_state(ColConnection *conn);
void connection_send(ColConnection *conn, Tuple *tuple);

#endif  /* CONNECTION_H */
