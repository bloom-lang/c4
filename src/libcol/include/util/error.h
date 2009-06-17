#ifndef ERROR_H
#define ERROR_H

#define FAIL()          simple_error(__FILE__, __LINE__)
#define ERROR(fmt, ...) var_error(__FILE__, __LINE__, fmt, __VA_ARGS__)
#define ASSERT(cond)    ((cond) || assert_fail(APR_STRINGIFY(cond),     \
                                               __FILE__, __LINE__))

void simple_error(const char *file, int line_num);
void var_error(const char *file, int line_num, const char *fmt, ...);
int assert_fail(const char *cond, const char *file, int line_num);

#endif  /* ERROR_H */
