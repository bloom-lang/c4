#ifndef ERROR_H
#define ERROR_H

/*
 * Note that we include "fmt" in the variadic argument list, because C99
 * apparently doesn't allow variadic macros to be invoked without any vargs
 * parameters.
 */
#define ERROR(...)      var_error(__FILE__, __LINE__, __VA_ARGS__)
#define FAIL()          simple_error(__FILE__, __LINE__)
#define ASSERT(cond)    ((cond) || assert_fail(APR_STRINGIFY(cond),     \
                                               __FILE__, __LINE__))

void simple_error(const char *file, int line_num);
void var_error(const char *file, int line_num, const char *fmt, ...);
int assert_fail(const char *cond, const char *file, int line_num);

#endif  /* ERROR_H */
