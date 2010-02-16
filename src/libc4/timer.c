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
    apr_pool_t *pool;
    C4Runtime *c4;
    AlarmState *alarm;
};

C4Timer *
timer_make(C4Runtime *c4)
{
    C4Timer *timer;

    timer = apr_palloc(c4->pool, sizeof(*timer));
    timer->pool = c4->pool;
    timer->c4 = c4;
    timer->alarm = NULL;
    return timer;
}

void
timer_add_alarm(C4Timer *timer, const char *name, apr_int64_t period_msec)
{
    AlarmState *alarm;

    alarm = apr_palloc(timer->pool, sizeof(*alarm));
    alarm->period = period_msec * 1000;
    alarm->tbl_def = cat_get_table(timer->c4->cat, name);
    alarm->next = timer->alarm;
    timer->alarm = alarm;
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
