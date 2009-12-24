#ifndef ERROR_H
#define ERROR_H

/*
 * Note that we include "fmt" in the variadic argument list, because C99
 * apparently doesn't allow variadic macros to be invoked without any vargs
 * parameters.
 */
#define ERROR(...)      var_error(__FILE__, __LINE__, __VA_ARGS__)
#define FAIL()          simple_error(__FILE__, __LINE__)
#define FAIL_APR(s)     apr_error((s), __FILE__, __LINE__)
#define FAIL_SQLITE(c)  sqlite_error((c), __FILE__, __LINE__)

#ifdef ASSERT_ENABLED
#define ASSERT(cond)    \
    do {                \
        if (!(cond))    \
            assert_fail(APR_STRINGIFY(cond), __FILE__, __LINE__);   \
    } while (0)
#else
#define ASSERT(cond)
#endif

void apr_error(apr_status_t s, const char *file, int line_num) __attribute__((noreturn));
void sqlite_error(C4Instance *c4, const char *file, int line_num) __attribute__((noreturn));
void simple_error(const char *file, int line_num) __attribute__((noreturn));
void var_error(const char *file, int line_num,
               const char *fmt, ...) __attribute__((format(printf, 3, 4), noreturn));

void assert_fail(const char *cond, const char *file, int line_num) __attribute__((noreturn));

#endif  /* ERROR_H */
