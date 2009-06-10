#include <stdlib.h>

#include "util/mem.h"

void *
ol_alloc(size_t sz)
{
    void *result = malloc(sz);
    if (result == NULL)
        exit(0);
    return result;
}

void *
ol_alloc0(size_t sz)
{
    void *result = ol_alloc(sz);
    memset(result, 0, sz);
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


