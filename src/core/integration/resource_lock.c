#include "resource_lock.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <errno.h>
#endif

#ifndef LOCK_MAX
#define LOCK_MAX 1024
#endif

typedef struct RogueMutex {
    int order_id;
    const char *name;
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mtx;
#endif
    RogueLockStats stats;
} RogueMutex;

typedef struct RogueRwLock {
    int order_id;
    const char *name;
#ifdef _WIN32
    SRWLOCK lock; // native slim rwlock
#else
    pthread_rwlock_t rw;
#endif
    RogueLockStats read_stats;
    RogueLockStats write_stats;
} RogueRwLock;

static struct {
    RogueMutex *mutexes[LOCK_MAX];
    RogueRwLock *rwlocks[LOCK_MAX];
    uint32_t mutex_count;
    uint32_t rwlock_count;
    uint64_t total_acq;
    uint64_t total_contention;
    uint64_t total_deadlock_prev;
    uint64_t total_timeout;
} g_lock_stats;

static uint64_t now_ns(void){
#if defined(_WIN32)
    LARGE_INTEGER freq, ctr; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&ctr); return (uint64_t)(ctr.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec*1000000000ULL + ts.tv_nsec;
#endif
}

static void record_acq(RogueLockStats *st, RogueLockPriority pri, uint64_t wait_ns, int contended){
    st->acquisitions++; st->priority_acq[pri]++; st->wait_time_ns += wait_ns; if(contended) st->contended_acquisitions++; }

// Thread-local simple lock order stack for deadlock prevention (LIFO ascending requirement)
#if defined(_WIN32)
__declspec(thread) static int tls_lock_stack[32];
__declspec(thread) static int tls_lock_top;
#else
static __thread int tls_lock_stack[32];
static __thread int tls_lock_top;
#endif

static int check_order_push(int order_id){
    if(tls_lock_top>0){
        int last = tls_lock_stack[tls_lock_top-1];
        if(order_id < last){
            return -1; // would violate ordering (potential deadlock cycle)
        }
    }
    if(tls_lock_top < 32){ tls_lock_stack[tls_lock_top++] = order_id; }
    return 0;
}
static void check_order_pop(int order_id){
    if(tls_lock_top>0){
        // pop if top matches (best-effort; ignore mismatch)
        if(tls_lock_stack[tls_lock_top-1]==order_id) tls_lock_top--; else {
            // search and compact (rare path)
            for(int i=0;i<tls_lock_top;i++) if(tls_lock_stack[i]==order_id){
                for(int j=i+1;j<tls_lock_top;j++) tls_lock_stack[j-1]=tls_lock_stack[j];
                tls_lock_top--; break; }
        }
    }
}

RogueMutex *rogue_mutex_create(int order_id, const char *name){
    RogueMutex *m = (RogueMutex*)calloc(1,sizeof(RogueMutex)); if(!m) return NULL; m->order_id=order_id; m->name=name?name:"mutex";
#ifdef _WIN32
    InitializeCriticalSection(&m->cs);
#else
    pthread_mutex_init(&m->mtx,NULL);
#endif
    if(g_lock_stats.mutex_count<LOCK_MAX) g_lock_stats.mutexes[g_lock_stats.mutex_count++]=m;
    return m;
}

void rogue_mutex_destroy(RogueMutex *m){ if(!m) return; 
#ifdef _WIN32
    DeleteCriticalSection(&m->cs);
#else
    pthread_mutex_destroy(&m->mtx);
#endif
    free(m); }

int rogue_mutex_acquire(RogueMutex *m, RogueLockPriority pri, int timeout_ms){ if(!m) return -1; uint64_t start=now_ns(); int contended=0; 
    int pushed = 0; if(check_order_push(m->order_id)!=0){ m->stats.failed_deadlocks++; g_lock_stats.total_deadlock_prev++; return -1; } pushed = 1;
#ifdef _WIN32
    // No timed CS; emulate try if timeout_ms==0
    if(timeout_ms==0){ if(!TryEnterCriticalSection(&m->cs)){ m->stats.failed_timeouts++; g_lock_stats.total_timeout++; if(pushed) check_order_pop(m->order_id); return -1; } }
    else { EnterCriticalSection(&m->cs); }
#else
    if(timeout_ms==0){ if(pthread_mutex_trylock(&m->mtx)!=0){ m->stats.failed_timeouts++; g_lock_stats.total_timeout++; if(pushed) check_order_pop(m->order_id); return -1; } }
    else {
    if(pthread_mutex_trylock(&m->mtx)!=0){ contended=1; if(timeout_ms<0){ pthread_mutex_lock(&m->mtx); } else { // timed wait via loop sleep
        int waited=0; while(pthread_mutex_trylock(&m->mtx)!=0){ if(waited>=timeout_ms){ m->stats.failed_timeouts++; g_lock_stats.total_timeout++; if(pushed) check_order_pop(m->order_id); return -1; }
#ifdef _WIN32
            Sleep(1);
#else
            struct timespec sl={0,1000000}; nanosleep(&sl,NULL);
#endif
            waited+=1; }
            } }
    }
#endif
    uint64_t end=now_ns(); record_acq(&m->stats,pri,end-start,contended); g_lock_stats.total_acq++; if(contended) g_lock_stats.total_contention++; return 0; }

void rogue_mutex_release(RogueMutex *m){ if(!m) return; 
#ifdef _WIN32
    LeaveCriticalSection(&m->cs);
#else
    pthread_mutex_unlock(&m->mtx);
#endif
    check_order_pop(m->order_id);
}

const char *rogue_mutex_name(const RogueMutex *m){ return m? m->name: ""; }
void rogue_mutex_get_stats(const RogueMutex *m, RogueLockStats *out){ if(!m||!out) return; *out=m->stats; }

RogueRwLock *rogue_rwlock_create(int order_id, const char *name){ RogueRwLock *l=(RogueRwLock*)calloc(1,sizeof(RogueRwLock)); if(!l) return NULL; l->order_id=order_id; l->name=name?name:"rwlock";
#ifdef _WIN32
    InitializeSRWLock(&l->lock);
#else
    pthread_rwlock_init(&l->rw,NULL);
#endif
    if(g_lock_stats.rwlock_count<LOCK_MAX) g_lock_stats.rwlocks[g_lock_stats.rwlock_count++]=l; return l; }

void rogue_rwlock_destroy(RogueRwLock *l){ if(!l) return; 
#ifndef _WIN32
    pthread_rwlock_destroy(&l->rw);
#endif
    free(l); }

static int rw_try_read(RogueRwLock *l){
#ifdef _WIN32
    return TryAcquireSRWLockShared(&l->lock)?0:-1;
#else
    return pthread_rwlock_tryrdlock(&l->rw);
#endif
}
static int rw_lock_read(RogueRwLock *l){
#ifdef _WIN32
    AcquireSRWLockShared(&l->lock); return 0;
#else
    return pthread_rwlock_rdlock(&l->rw);
#endif
}
static int rw_try_write(RogueRwLock *l){
#ifdef _WIN32
    return TryAcquireSRWLockExclusive(&l->lock)?0:-1;
#else
    return pthread_rwlock_trywrlock(&l->rw);
#endif
}
static int rw_lock_write(RogueRwLock *l){
#ifdef _WIN32
    AcquireSRWLockExclusive(&l->lock); return 0;
#else
    return pthread_rwlock_wrlock(&l->rw);
#endif
}

int rogue_rwlock_acquire_read(RogueRwLock *l, RogueLockPriority pri, int timeout_ms){ if(!l) return -1; uint64_t start=now_ns(); int contended=0; if(timeout_ms==0){ if(rw_try_read(l)!=0){ l->read_stats.failed_timeouts++; g_lock_stats.total_timeout++; return -1; } }
    else { if(rw_try_read(l)!=0){ contended=1; if(timeout_ms<0){ rw_lock_read(l); } else { int waited=0; while(rw_try_read(l)!=0){ if(waited>=timeout_ms){ l->read_stats.failed_timeouts++; g_lock_stats.total_timeout++; return -1; }
#ifdef _WIN32
                Sleep(1);
#else
                struct timespec sl={0,1000000}; nanosleep(&sl,NULL);
#endif
                waited+=1; } } } }
    uint64_t end=now_ns(); record_acq(&l->read_stats,pri,end-start,contended); g_lock_stats.total_acq++; if(contended) g_lock_stats.total_contention++; return 0; }
int rogue_rwlock_acquire_write(RogueRwLock *l, RogueLockPriority pri, int timeout_ms){ if(!l) return -1; uint64_t start=now_ns(); int contended=0; if(check_order_push(l->order_id)!=0){ l->write_stats.failed_deadlocks++; g_lock_stats.total_deadlock_prev++; return -1; } if(timeout_ms==0){ if(rw_try_write(l)!=0){ l->write_stats.failed_timeouts++; g_lock_stats.total_timeout++; check_order_pop(l->order_id); return -1; } }
    else { if(rw_try_write(l)!=0){ contended=1; if(timeout_ms<0){ rw_lock_write(l); } else { int waited=0; while(rw_try_write(l)!=0){ if(waited>=timeout_ms){ l->write_stats.failed_timeouts++; g_lock_stats.total_timeout++; check_order_pop(l->order_id); return -1; }
#ifdef _WIN32
                Sleep(1);
#else
                struct timespec sl={0,1000000}; nanosleep(&sl,NULL);
#endif
                waited+=1; } } } }
    uint64_t end=now_ns(); record_acq(&l->write_stats,pri,end-start,contended); g_lock_stats.total_acq++; if(contended) g_lock_stats.total_contention++; return 0; }
void rogue_rwlock_release_read(RogueRwLock *l){ if(!l) return; 
#ifdef _WIN32
    ReleaseSRWLockShared(&l->lock);
#else
    pthread_rwlock_unlock(&l->rw);
#endif
}
void rogue_rwlock_release_write(RogueRwLock *l){ if(!l) return; 
#ifdef _WIN32
    ReleaseSRWLockExclusive(&l->lock);
#else
    pthread_rwlock_unlock(&l->rw);
#endif
    check_order_pop(l->order_id);
}
void rogue_rwlock_get_stats(const RogueRwLock *l, RogueLockStats *read_out, RogueLockStats *write_out){ if(!l) return; if(read_out) *read_out=l->read_stats; if(write_out) *write_out=l->write_stats; }

void rogue_lock_global_stats(RogueGlobalLockStats *out){ if(!out) return; out->mutex_count=g_lock_stats.mutex_count; out->rwlock_count=g_lock_stats.rwlock_count; out->total_acquisitions=g_lock_stats.total_acq; out->total_contentions=g_lock_stats.total_contention; out->total_deadlock_preventions=g_lock_stats.total_deadlock_prev; out->total_timeouts=g_lock_stats.total_timeout; }

void rogue_lock_dump(void *fptr){ FILE *f=fptr? (FILE*)fptr: stdout; fprintf(f,"[locks] mutexes=%u rwlocks=%u acq=%llu contention=%llu timeouts=%llu deadlock_prev=%llu\n", g_lock_stats.mutex_count,g_lock_stats.rwlock_count,(unsigned long long)g_lock_stats.total_acq,(unsigned long long)g_lock_stats.total_contention,(unsigned long long)g_lock_stats.total_timeout,(unsigned long long)g_lock_stats.total_deadlock_prev); for(uint32_t i=0;i<g_lock_stats.mutex_count;i++){ RogueMutex *m=g_lock_stats.mutexes[i]; fprintf(f," mutex '%s' acq=%llu contended=%llu timeouts=%llu wait_ns=%llu\n", m->name,(unsigned long long)m->stats.acquisitions,(unsigned long long)m->stats.contended_acquisitions,(unsigned long long)m->stats.failed_timeouts,(unsigned long long)m->stats.wait_time_ns); } for(uint32_t i=0;i<g_lock_stats.rwlock_count;i++){ RogueRwLock *l=g_lock_stats.rwlocks[i]; fprintf(f," rwlock '%s' r_acq=%llu w_acq=%llu r_cont=%llu w_cont=%llu r_timeout=%llu w_timeout=%llu\n", l->name,(unsigned long long)l->read_stats.acquisitions,(unsigned long long)l->write_stats.acquisitions,(unsigned long long)l->read_stats.contended_acquisitions,(unsigned long long)l->write_stats.contended_acquisitions,(unsigned long long)l->read_stats.failed_timeouts,(unsigned long long)l->write_stats.failed_timeouts); } }

void rogue_lock_reset_stats(void){ memset(&g_lock_stats,0,sizeof(g_lock_stats)); tls_lock_top=0; }
