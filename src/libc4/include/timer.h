#ifndef TIMER_H
#define TIMER_H

typedef struct C4Timer C4Timer;

C4Timer *timer_make(C4Runtime *c4);
void timer_add_alarm(C4Timer *timer, const char *name,
                     apr_int64_t period_msec);
apr_interval_time_t timer_get_deadline(C4Timer *timer);
void timer_invoke(C4Timer *timer);

#endif  /* TIMER_H */
