#ifndef LOGGER_H
#define LOGGER_H

#include "types/tuple.h"

typedef struct ColLogger ColLogger;

ColLogger *logger_make(ColInstance *col);
void col_log(ColInstance *col, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
char *log_tuple(ColInstance *col, Tuple *tuple);

#endif  /* LOGGER_H */
