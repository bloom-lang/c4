#include <apr_thread_cond.h>
#include <apr_thread_mutex.h>

#include "c4-internal.h"
#include "util/thread_sync.h"

struct C4ThreadSync
{
    apr_thread_mutex_t *lock;
    apr_thread_cond_t *cond;
    enum
    {
        SYNC_IDLE,
        SYNC_PREPARED,
        SYNC_WAITING,
        SYNC_SIGNALED
    } state;
};

C4ThreadSync *
thread_sync_make(apr_pool_t *pool)
{
    C4ThreadSync *ts;
    apr_status_t s;

    ts = apr_palloc(pool, sizeof(*ts));
    ts->state = SYNC_IDLE;
    s = apr_thread_mutex_create(&ts->lock,
                                APR_THREAD_MUTEX_DEFAULT,
                                pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_thread_cond_create(&ts->cond, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return ts;
}

/*
 * Prepare a ThreadSync to be waited-upon. This should occur shortly before the
 * _wait() call, and also before another thread might potentially signal the
 * ThreadSync().
 */
void
thread_sync_prepare(C4ThreadSync *ts)
{
    apr_thread_mutex_lock(ts->lock);
    ASSERT(ts->state == SYNC_IDLE);
    ts->state = SYNC_PREPARED;
    /* We continue to hold the lock */
}

void
thread_sync_wait(C4ThreadSync *ts)
{
    /* Lock is held on entry */
    ASSERT(ts->state == SYNC_PREPARED);
    ts->state = SYNC_WAITING;

    do {
        apr_thread_cond_wait(ts->cond, ts->lock);
    } while (ts->state != SYNC_SIGNALED);

    ts->state = SYNC_IDLE;
    apr_thread_mutex_unlock(ts->lock);
}

void
thread_sync_signal(C4ThreadSync *ts)
{
    apr_thread_mutex_lock(ts->lock);
    ASSERT(ts->state == SYNC_WAITING);
    ts->state = SYNC_SIGNALED;
    apr_thread_cond_signal(ts->cond);
    apr_thread_mutex_unlock(ts->lock);
}
