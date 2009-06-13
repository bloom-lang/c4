#ifndef ERROR_H
#define ERROR_H

#define FAIL()          fatal_error(__FILE__, __LINE__)
#define ASSERT(cond)    (!(cond) || assert_fail(APR_STRINGIFY(cond), \
                                                __FILE__, __LINE__))

void fatal_error(const char *file, int line_num);
int assert_fail(const char *cond, const char *file, int line_num);

#endif  /* ERROR_H */
