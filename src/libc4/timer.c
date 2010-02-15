#include "c4-internal.h"
#include "timer.h"

typedef struct AlarmState
{
    apr_interval_time_t period;
    apr_interval_time_t deadline;
    TableDef *tbl_def;
    struct AlarmState *next;
} AlarmState;

struct C4Timer
{
    AlarmState *alarm;
    AlarmState *free_alarm;
};

C4Timer *
timer_make(apr_pool_t *pool)
{
    C4Timer *timer;

    timer = apr_palloc(pool, sizeof(*timer));
    timer->alarm = NULL;
    timer->free_alarm = NULL;
    return timer;
}

void
timer_add_alarm(C4Timer *timer)
{
    ;
}

apr_interval_time_t
timer_get_deadline(C4Timer *timer)
{
    return -1;
}

void
timer_invoke(C4Timer *timer)
{
    ;
}
