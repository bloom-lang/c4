#ifndef SOCKET_H
#define SOCKET_H

#include <apr_network_io.h>

void socket_set_non_block(apr_socket_t *sock);

apr_sockaddr_t *socket_get_remote_addr(apr_socket_t *sock);
char *socket_get_remote_loc(apr_socket_t *sock, apr_pool_t *pool);

#endif  /* SOCKET_H */

