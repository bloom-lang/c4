#ifndef STR_H
#define STR_H

char *downcase_buf(const char *str, apr_size_t len, apr_pool_t *pool);
char *downcase_str(const char *str, apr_pool_t *pool);

#endif  /* STR_H */
