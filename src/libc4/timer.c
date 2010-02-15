#include "c4-internal.h"
#include "timer.h"

struct C4Timer
{
};

C4Timer *
c4_timer_make(apr_pool_t *pool)
{
    C4Timer *timer;

    timer = apr_palloc(pool, sizeof(*timer));
    return timer;
}
