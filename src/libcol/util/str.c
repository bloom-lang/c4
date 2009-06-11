#include <ctype.h>

#include "util/mem.h"
#include "util/str.h"

char *
downcase_str(const char *str, int len)
{
    char *result;
    int   i;

    result = ol_alloc(len + 1);
    for (i = 0; i < len; i++)
    {
        unsigned char ch = (unsigned char) str[i];

        if (isupper(ch))
            ch = tolower(ch);

        result[i] = (char) ch;
    }
    result[i] = '\0';

    return result;
}
