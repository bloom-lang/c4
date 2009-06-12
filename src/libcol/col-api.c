#include "col-api.h"
#include "col-internal.h"
#include "util/mem.h"

ColInstance *
col_init()
{
    ColInstance *result = ol_alloc(sizeof(*result));
    return result;
}

ColStatus
col_shutdown(ColInstance *col)
{
    ol_free(col);
    return COL_OK;
}

ColStatus
col_install_file(const char *path)
{
    return COL_OK;
}

ColStatus
col_install_str(const char *str)
{
    return COL_OK;
}
