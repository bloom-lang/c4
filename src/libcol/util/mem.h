#ifndef MEM_H
#define MEM_H

void *ol_alloc(size_t sz);
void *ol_alloc0(size_t sz);
void ol_free(void *ptr);
char *ol_strdup(const char *str);

#endif  /* MEM_H */
