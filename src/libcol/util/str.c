#include <ctype.h>

#include "col-internal.h"
#include "util/str.h"

char *
downcase_buf(const char *buf, apr_size_t len, apr_pool_t *pool)
{
    char *result;
    apr_size_t i;

    result = apr_palloc(pool, len + 1);
    for (i = 0; i < len; i++)
    {
        unsigned char ch = (unsigned char) buf[i];

        if (isupper(ch))
            ch = tolower(ch);

        result[i] = (char) ch;
    }
    result[i] = '\0';

    return result;
}

char *
downcase_str(const char *str, apr_pool_t *pool)
{
    return downcase_buf(str, strlen(str), pool);
}
