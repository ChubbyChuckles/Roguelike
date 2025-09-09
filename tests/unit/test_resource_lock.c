#include "resource_lock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_mutex_basic()
{
    rogue_lock_reset_stats();
    RogueMutex* m = rogue_mutex_create(10, "test_mtx");
    if (!m)
    {
        printf("mutex create failed\n");
        return;
    }
    if (rogue_mutex_acquire(m, ROGUE_LOCK_PRIORITY_NORMAL, -1) != 0)
    {
        printf("mutex acquire failed\n");
        rogue_mutex_destroy(m);
        return;
    }
    RogueLockStats st;
    rogue_mutex_get_stats(m, &st);
    rogue_mutex_release(m);
    if (!(st.acquisitions == 1 && st.failed_timeouts == 0))
    {
        printf("unexpected stats acq=%u timeouts=%u\n", st.acquisitions, st.failed_timeouts);
    }
    rogue_mutex_destroy(m);
}

// Cross-thread contention try-acquire test
#ifdef _WIN32
#include <windows.h>
static volatile LONG holder_acquired = 0;
DWORD WINAPI hold_mutex_thread(LPVOID param)
{
    RogueMutex* m = (RogueMutex*) param;
    if (rogue_mutex_acquire(m, ROGUE_LOCK_PRIORITY_NORMAL, -1) == 0)
    {
        InterlockedExchange(&holder_acquired, 1);
        Sleep(50);
        rogue_mutex_release(m);
    }
    return 0;
}
#else
#include <pthread.h>
#include <unistd.h>
static volatile int holder_acquired = 0;
static void* hold_mutex_thread(void* param)
{
    RogueMutex* m = (RogueMutex*) param;
    if (rogue_mutex_acquire(m, ROGUE_LOCK_PRIORITY_NORMAL, -1) == 0)
    {
        holder_acquired = 1;
        usleep(50 * 1000);
        rogue_mutex_release(m);
    }
    return NULL;
}
#endif

static void test_mutex_try_contention()
{
    rogue_lock_reset_stats();
    RogueMutex* m = rogue_mutex_create(20, "busy");
    if (!m)
    {
        printf("mutex create failed\n");
        return;
    }
    // Launch thread that acquires mutex
#ifdef _WIN32
    HANDLE th = CreateThread(NULL, 0, hold_mutex_thread, m, 0, NULL);
    if (!th)
    {
        printf("CreateThread failed\n");
        rogue_mutex_destroy(m);
        return;
    }
#else
    pthread_t th;
    assert(pthread_create(&th, NULL, hold_mutex_thread, m) == 0);
#endif
    // Wait until the holder thread definitely acquired the mutex
    int spin = 0;
    int rc_try;
#ifdef _WIN32
    while (holder_acquired == 0 && spin < 200)
    {
        Sleep(1);
        spin++;
    }
#else
    while (holder_acquired == 0 && spin < 200)
    {
        usleep(1000);
        spin++;
    }
#endif
    rc_try = rogue_mutex_acquire(m, ROGUE_LOCK_PRIORITY_BACKGROUND, 0);
    if (rc_try == 0)
    {
        printf("try-acquire unexpectedly succeeded during contention\n");
    }
#ifdef _WIN32
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th);
#else
    pthread_join(th, NULL);
#endif
    // Now blocking acquire succeeds
    if (rogue_mutex_acquire(m, ROGUE_LOCK_PRIORITY_NORMAL, -1) != 0)
    {
        printf("mutex acquire post-join failed\n");
        rogue_mutex_destroy(m);
        return;
    }
    rogue_mutex_release(m);
    rogue_mutex_destroy(m);
}

static void test_mutex_ordering()
{
    rogue_lock_reset_stats();
    RogueMutex* low = rogue_mutex_create(5, "low");
    RogueMutex* high = rogue_mutex_create(15, "high");
    if (!low || !high)
    {
        printf("mutex create low/high failed\n");
        if (low)
            rogue_mutex_destroy(low);
        if (high)
            rogue_mutex_destroy(high);
        return;
    }
    // Increasing order ok
    if (rogue_mutex_acquire(low, ROGUE_LOCK_PRIORITY_NORMAL, -1) != 0 ||
        rogue_mutex_acquire(high, ROGUE_LOCK_PRIORITY_NORMAL, -1) != 0)
    {
        printf("mutex acquire low/high failed\n");
    }
    rogue_mutex_release(high);
    rogue_mutex_release(low);
    // Decreasing order should fail (deadlock prevention)
    if (rogue_mutex_acquire(high, ROGUE_LOCK_PRIORITY_NORMAL, -1) != 0)
    {
        printf("mutex acquire high failed\n");
    }
    int rc = rogue_mutex_acquire(low, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    if (rc == 0)
    {
        printf("expected deadlock prevention\n");
    }
    RogueLockStats st_low;
    rogue_mutex_get_stats(low, &st_low);
    if (st_low.failed_deadlocks != 1)
    {
        printf("unexpected failed_deadlocks=%u\n", st_low.failed_deadlocks);
    }
    rogue_mutex_release(high);
    rogue_mutex_destroy(low);
    rogue_mutex_destroy(high);
}

static void test_rwlock()
{
    rogue_lock_reset_stats();
    RogueRwLock* l = rogue_rwlock_create(30, "test_rw");
    if (!l)
    {
        printf("rwlock create failed\n");
        return;
    }
    // Two readers allowed
    if (rogue_rwlock_acquire_read(l, ROGUE_LOCK_PRIORITY_NORMAL, -1) != 0 ||
        rogue_rwlock_acquire_read(l, ROGUE_LOCK_PRIORITY_BACKGROUND, -1) != 0)
    {
        printf("rwlock read acquire failed\n");
    }
    rogue_rwlock_release_read(l);
    rogue_rwlock_release_read(l);
    // Writer
    if (rogue_rwlock_acquire_write(l, ROGUE_LOCK_PRIORITY_CRITICAL, -1) != 0)
    {
        printf("rwlock write acquire failed\n");
    }
    rogue_rwlock_release_write(l);
    RogueLockStats r, w;
    rogue_rwlock_get_stats(l, &r, &w);
    if (!(r.acquisitions == 2 && w.acquisitions == 1))
    {
        printf("unexpected rw stats r=%u w=%u\n", r.acquisitions, w.acquisitions);
    }
    rogue_rwlock_destroy(l);
}

int main()
{
    test_mutex_basic();
    test_mutex_try_contention();
    test_mutex_ordering();
    test_rwlock();
    RogueGlobalLockStats g;
    rogue_lock_global_stats(&g);
    printf("[resource_lock] mutex=%u rw=%u acq=%llu cont=%llu timeouts=%llu\n", g.mutex_count,
           g.rwlock_count, (unsigned long long) g.total_acquisitions,
           (unsigned long long) g.total_contentions, (unsigned long long) g.total_timeouts);
    return 0;
}
