#ifndef THREAD_SYNC_H
#define THREAD_SYNC_H

typedef struct C4ThreadSync C4ThreadSync;

C4ThreadSync *thread_sync_make(apr_pool_t *pool);
void thread_sync_prepare(C4ThreadSync *ts);
void thread_sync_wait(C4ThreadSync *ts);
void thread_sync_signal(C4ThreadSync *ts);

#endif  /* THREAD_SYNC_H */
