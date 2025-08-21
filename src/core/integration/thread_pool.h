#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <SDL.h> // for SDL_Thread
#include "threading.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*RogueTaskFn)(void *user);

typedef struct RogueTask {
    RogueTaskFn fn;
    void *user;
} RogueTask;

// Fixed-size SPSC ring for tasks (lock-free for 1 producer, 1 consumer)
#define ROGUE_TP_RING 1024

typedef struct RogueThreadPool {
    int           thread_count;
    SDL_Thread  **threads;
    SDL_atomic_t head; // producer index
    SDL_atomic_t tail; // consumer index
    RogueTask     ring[ROGUE_TP_RING];
    RogueSem      work_sem; // counts pending tasks
    volatile int  running;
    // stats & tuning
    SDL_atomic_t  tasks_submitted;
    SDL_atomic_t  tasks_executed;
    SDL_atomic_t  worker_wakeups;
    SDL_atomic_t  peak_queue;
    int           debug_yield;
    SDL_ThreadPriority priority;
} RogueThreadPool;

typedef struct RogueThreadPoolStats {
    uint32_t pending;
    uint32_t peak_queue;
    uint32_t worker_wakeups;
    uint32_t tasks_submitted;
    uint32_t tasks_executed;
} RogueThreadPoolStats;

int  rogue_thread_pool_init(RogueThreadPool *tp, int threads);
void rogue_thread_pool_shutdown(RogueThreadPool *tp);
int  rogue_thread_pool_submit(RogueThreadPool *tp, RogueTaskFn fn, void *user);
int  rogue_thread_pool_pending(RogueThreadPool *tp);
void rogue_thread_pool_get_stats(RogueThreadPool *tp, RogueThreadPoolStats *out);
void rogue_thread_pool_set_priority(RogueThreadPool *tp, SDL_ThreadPriority pri);
void rogue_thread_pool_set_debug_yield(RogueThreadPool *tp, bool enable);

#ifdef __cplusplus
}
#endif
