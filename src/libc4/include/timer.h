#ifndef TIMER_H
#define TIMER_H

#include "util/thread_sync.h"

typedef struct C4Timer C4Timer;

C4Timer *c4_timer_start(C4ThreadSync *thread_sync, apr_pool_t *pool);

#endif  /* TIMER_H */
