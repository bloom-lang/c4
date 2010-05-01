#ifndef TIMER_H
#define TIMER_H

typedef struct C4Timer C4Timer;

C4Timer *timer_make(C4Runtime *c4);
void timer_add_alarm(C4Timer *timer, const char *name,
                     apr_int64_t period_msec);
apr_interval_time_t timer_get_sleep_time(C4Timer *timer);
bool timer_poll(C4Timer *timer);

#endif  /* TIMER_H */
