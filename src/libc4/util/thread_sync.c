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

void
thread_sync_wait(C4ThreadSync *ts)
{
    apr_status_t s;

    s = apr_thread_mutex_lock(ts->lock);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    ASSERT(ts->state != SYNC_WAITING);

    /* If we've already been signaled, no need to wait() */
    if (ts->state == SYNC_IDLE)
    {
        ts->state = SYNC_WAITING;

        do {
            s = apr_thread_cond_wait(ts->cond, ts->lock);
            if (s != APR_SUCCESS)
                FAIL_APR(s);
        } while (ts->state != SYNC_SIGNALED);
    }

    ts->state = SYNC_IDLE;
    s = apr_thread_mutex_unlock(ts->lock);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}

void
thread_sync_signal(C4ThreadSync *ts)
{
    apr_status_t s;

    s = apr_thread_mutex_lock(ts->lock);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    ASSERT(ts->state != SYNC_SIGNALED);

    if (ts->state == SYNC_WAITING)
    {
        s = apr_thread_cond_signal(ts->cond);
        if (s != APR_SUCCESS)
            FAIL_APR(s);
    }
    ts->state = SYNC_SIGNALED;
    s = apr_thread_mutex_unlock(ts->lock);
    if (s != APR_SUCCESS)
        FAIL_APR(s);
}
