#include "col-internal.h"
#include "net/network.h"

ColNetwork *
network_init()
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
