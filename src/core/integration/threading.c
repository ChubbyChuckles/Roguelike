#include "threading.h"
#include "../../util/log.h"

#include <SDL.h>
#include <stdlib.h>

int rogue_sem_init(RogueSem* s, uint32_t initial)
{
    if (!s)
        return -1;
    s->sem = SDL_CreateSemaphore(initial);
    return s->sem ? 0 : -1;
}
void rogue_sem_destroy(RogueSem* s)
{
    if (!s || !s->sem)
        return;
    SDL_DestroySemaphore(s->sem);
    s->sem = NULL;
}
int rogue_sem_post(RogueSem* s) { return (!s || !s->sem) ? -1 : SDL_SemPost(s->sem); }
int rogue_sem_wait(RogueSem* s) { return (!s || !s->sem) ? -1 : SDL_SemWait(s->sem); }
int rogue_sem_trywait(RogueSem* s) { return (!s || !s->sem) ? -1 : SDL_SemTryWait(s->sem); }

int rogue_cond_init(RogueCond* c)
{
    if (!c)
        return -1;
    c->mtx = SDL_CreateMutex();
    c->cond = SDL_CreateCond();
    return (c->mtx && c->cond) ? 0 : -1;
}
void rogue_cond_destroy(RogueCond* c)
{
    if (!c)
        return;
    if (c->cond)
        SDL_DestroyCond(c->cond);
    if (c->mtx)
        SDL_DestroyMutex(c->mtx);
    c->cond = NULL;
    c->mtx = NULL;
}
void rogue_cond_lock(RogueCond* c)
{
    if (c && c->mtx)
        SDL_LockMutex(c->mtx);
}
void rogue_cond_unlock(RogueCond* c)
{
    if (c && c->mtx)
        SDL_UnlockMutex(c->mtx);
}
int rogue_cond_wait(RogueCond* c)
{
    return (!c || !c->cond || !c->mtx) ? -1 : SDL_CondWait(c->cond, c->mtx);
}
int rogue_cond_signal(RogueCond* c) { return (!c || !c->cond) ? -1 : SDL_CondSignal(c->cond); }
int rogue_cond_broadcast(RogueCond* c)
{
    return (!c || !c->cond) ? -1 : SDL_CondBroadcast(c->cond);
}

int rogue_barrier_init(RogueBarrier* b, uint32_t count)
{
    if (!b || count == 0)
        return -1;
    b->mtx = SDL_CreateMutex();
    b->cond = SDL_CreateCond();
    b->count = count;
    b->waiting = 0;
    b->generation = 0;
    return (b->mtx && b->cond) ? 0 : -1;
}
void rogue_barrier_destroy(RogueBarrier* b)
{
    if (!b)
        return;
    if (b->cond)
        SDL_DestroyCond(b->cond);
    if (b->mtx)
        SDL_DestroyMutex(b->mtx);
    b->cond = NULL;
    b->mtx = NULL;
    b->count = b->waiting = b->generation = 0;
}
uint32_t rogue_barrier_wait(RogueBarrier* b)
{
    if (!b || !b->mtx || !b->cond)
        return 0;
    SDL_LockMutex(b->mtx);
    uint32_t gen = b->generation;
    b->waiting++;
    if (b->waiting == b->count)
    {
        b->generation++;
        b->waiting = 0;
        SDL_CondBroadcast(b->cond);
        SDL_UnlockMutex(b->mtx);
        return b->generation;
    }
    while (gen == b->generation)
    {
        SDL_CondWait(b->cond, b->mtx);
    }
    SDL_UnlockMutex(b->mtx);
    return b->generation;
}
