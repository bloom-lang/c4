#include "col-api.h"
#include "col-internal.h"
#include "util/mem.h"

ColInstance *
col_init()
{
    ColInstance *result = ol_alloc(sizeof(*result));
    return result;
}

void
col_shutdown(ColInstance *col)
{
    ol_free(col);
}
