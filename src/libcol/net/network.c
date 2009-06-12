#include "col-internal.h"
#include "net/network.h"

/*
 * Create a new instance of the network interface. "port" is the local TCP
 * port to listen on; 0 means to use an ephemeral port.
 */
ColNetwork *
network_make(int port)
{
    ColNetwork *result = ol_alloc(sizeof(*result));
    return result;
}

ColStatus
network_destroy(ColNetwork *net)
{
    ol_free(net);
    return COL_OK;
}

void
network_start(ColNetwork *net)
{
    ;
}
