#include "resource_lock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_mutex_basic()
{
    rogue_lock_reset_stats();
    RogueMutex* m = rogue_mutex_create(10, "test_mtx");
    assert(m);
    assert(rogue_mutex_acquire(m, ROGUE_LOCK_PRIORITY_NORMAL, -1) == 0);
    RogueLockStats st;
    rogue_mutex_get_stats(m, &st);
    rogue_mutex_release(m);
    assert(st.acquisitions == 1 && st.failed_timeouts == 0);
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
    assert(m);
    // Launch thread that acquires mutex
#ifdef _WIN32
    HANDLE th = CreateThread(NULL, 0, hold_mutex_thread, m, 0, NULL);
    assert(th);
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
    assert(rc_try != 0); // should have failed at least once while other thread held it
#ifdef _WIN32
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th);
#else
    pthread_join(th, NULL);
#endif
    // Now blocking acquire succeeds
    assert(rogue_mutex_acquire(m, ROGUE_LOCK_PRIORITY_NORMAL, -1) == 0);
    rogue_mutex_release(m);
    rogue_mutex_destroy(m);
}

static void test_mutex_ordering()
{
    rogue_lock_reset_stats();
    RogueMutex* low = rogue_mutex_create(5, "low");
    RogueMutex* high = rogue_mutex_create(15, "high");
    assert(low && high);
    // Increasing order ok
    assert(rogue_mutex_acquire(low, ROGUE_LOCK_PRIORITY_NORMAL, -1) == 0);
    assert(rogue_mutex_acquire(high, ROGUE_LOCK_PRIORITY_NORMAL, -1) == 0);
    rogue_mutex_release(high);
    rogue_mutex_release(low);
    // Decreasing order should fail (deadlock prevention)
    assert(rogue_mutex_acquire(high, ROGUE_LOCK_PRIORITY_NORMAL, -1) == 0);
    int rc = rogue_mutex_acquire(low, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    assert(rc != 0); // should be prevented
    RogueLockStats st_low;
    rogue_mutex_get_stats(low, &st_low);
    assert(st_low.failed_deadlocks == 1);
    rogue_mutex_release(high);
    rogue_mutex_destroy(low);
    rogue_mutex_destroy(high);
}

static void test_rwlock()
{
    rogue_lock_reset_stats();
    RogueRwLock* l = rogue_rwlock_create(30, "test_rw");
    assert(l);
    // Two readers allowed
    assert(rogue_rwlock_acquire_read(l, ROGUE_LOCK_PRIORITY_NORMAL, -1) == 0);
    assert(rogue_rwlock_acquire_read(l, ROGUE_LOCK_PRIORITY_BACKGROUND, -1) == 0);
    rogue_rwlock_release_read(l);
    rogue_rwlock_release_read(l);
    // Writer
    assert(rogue_rwlock_acquire_write(l, ROGUE_LOCK_PRIORITY_CRITICAL, -1) == 0);
    rogue_rwlock_release_write(l);
    RogueLockStats r, w;
    rogue_rwlock_get_stats(l, &r, &w);
    assert(r.acquisitions == 2 && w.acquisitions == 1);
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
