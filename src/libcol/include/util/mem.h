#ifndef MEM_H
#define MEM_H

#include <stddef.h>

void *ol_alloc(size_t sz);
void *ol_alloc0(size_t sz);
void ol_free(void *ptr);
char *ol_strdup(const char *str);

apr_pool_t *make_subpool(apr_pool_t *parent);

#endif  /* MEM_H */
