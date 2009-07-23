#ifndef ERROR_H
#define ERROR_H

/*
 * Note that we include "fmt" in the variadic argument list, because C99
 * apparently doesn't allow variadic macros to be invoked without any vargs
 * parameters.
 */
#define ERROR(...)      var_error(__FILE__, __LINE__, __VA_ARGS__)
#define FAIL()          simple_error(__FILE__, __LINE__)

#define ASSERT(cond)    \
    do {                \
        if (!(cond))    \
            assert_fail(APR_STRINGIFY(cond), __FILE__, __LINE__);   \
    } while (0)

void simple_error(const char *file, int line_num) __attribute__((noreturn));
void var_error(const char *file, int line_num,
               const char *fmt, ...) __attribute__((format(printf, 3, 4), noreturn));
void assert_fail(const char *cond, const char *file, int line_num) __attribute__((noreturn));

#endif  /* ERROR_H */
