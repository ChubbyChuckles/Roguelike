// Simple cross-platform threading primitives built on SDL2 (C only)
// Provides: semaphore, condition variable, barrier helpers
// Note: We already have resource_lock for ordered mutex/rwlocks. This module covers
// portable primitives used by the thread pool and tests.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declare SDL types to avoid forcing global include dependence here.
typedef struct SDL_mutex SDL_mutex;
typedef struct SDL_cond SDL_cond;
typedef struct SDL_semaphore SDL_sem;

typedef struct RogueSem {
    SDL_sem *sem;
} RogueSem;

typedef struct RogueCond {
    SDL_mutex *mtx;
    SDL_cond  *cond;
} RogueCond;

typedef struct RogueBarrier {
    SDL_mutex *mtx;
    SDL_cond  *cond;
    uint32_t   count;
    uint32_t   waiting;
    uint32_t   generation;
} RogueBarrier;

// Semaphore
int  rogue_sem_init(RogueSem *s, uint32_t initial);
void rogue_sem_destroy(RogueSem *s);
int  rogue_sem_post(RogueSem *s);
int  rogue_sem_wait(RogueSem *s);        // blocks until >0
int  rogue_sem_trywait(RogueSem *s);     // returns 0 on success, non-zero otherwise

// Condition variable + internal mutex
int  rogue_cond_init(RogueCond *c);
void rogue_cond_destroy(RogueCond *c);
void rogue_cond_lock(RogueCond *c);
void rogue_cond_unlock(RogueCond *c);
int  rogue_cond_wait(RogueCond *c);      // waits and re-acquires lock
int  rogue_cond_signal(RogueCond *c);
int  rogue_cond_broadcast(RogueCond *c);

// Barrier (Pthreads-like) — waits for N participants; reusable
int  rogue_barrier_init(RogueBarrier *b, uint32_t count);
void rogue_barrier_destroy(RogueBarrier *b);
// Returns generation index when passing the barrier
uint32_t rogue_barrier_wait(RogueBarrier *b);

#ifdef __cplusplus
}
#endif
