#ifndef SOCKET_H
#define SOCKET_H

#include <apr_network_io.h>

apr_uint32_t socket_recv_uint32(apr_socket_t *sock, bool *is_eof);
apr_size_t socket_recv_data(apr_socket_t *sock, char *buf,
                            apr_size_t buf_len, bool *is_eof);

void socket_send_uint32(apr_socket_t *sock, apr_uint32_t u32);
void socket_send_data(apr_socket_t *sock, const char *buf, apr_size_t buf_len);
void socket_send_str(apr_socket_t *sock, const char *str);

void socket_set_non_block(apr_socket_t *sock);

apr_sockaddr_t *socket_get_remote_addr(apr_socket_t *sock);
char *socket_get_remote_loc(apr_socket_t *sock, apr_pool_t *pool);

#endif  /* SOCKET_H */

