#include <string.h>

#include "c4-internal.h"

void *
ol_alloc(apr_size_t sz)
{
    void *result = malloc(sz);
    if (result == NULL)
        FAIL();
    return result;
}

void *
ol_alloc0(apr_size_t sz)
{
    void *result = ol_alloc(sz);
    memset(result, 0, sz);
    return result;
}

void *
ol_realloc(void *ptr, apr_size_t sz)
{
    void *result = realloc(ptr, sz);
    if (result == NULL)
        FAIL();
    return result;
}

void
ol_free(void *ptr)
{
    free(ptr);
}

char *
ol_strdup(const char *str)
{
    size_t len = strlen(str) + 1;
    char *result;

    result = ol_alloc(len);
    memcpy(result, str, len);

    return result;
}

apr_pool_t *
make_subpool(apr_pool_t *parent)
{
    apr_status_t s;
    apr_pool_t *pool;

    s = apr_pool_create(&pool, parent);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return pool;
}
