#include "thread_pool.h"
#include "../../util/log.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

static int worker_main(void *ud){ RogueThreadPool *tp=(RogueThreadPool*)ud; while(tp->running){
    if(rogue_sem_wait(&tp->work_sem)!=0) break; if(!tp->running) break;
    SDL_SetThreadPriority(tp->priority);
    SDL_AtomicAdd(&tp->worker_wakeups,1);
        RogueTask task = {0};
        for(;;){
            uint32_t t = (uint32_t)SDL_AtomicGet(&tp->tail);
            uint32_t h = (uint32_t)SDL_AtomicGet(&tp->head);
            if(t==h) break; // no task visible yet
            task = tp->ring[t & (ROGUE_TP_RING-1)];
            // advance tail
            if(SDL_AtomicCAS(&tp->tail,(int)t,(int)(t+1))) break; // claimed
        }
    if(task.fn){ if(tp->debug_yield) SDL_Delay(0); task.fn(task.user); SDL_AtomicAdd(&tp->tasks_executed,1); }
    } return 0; }

static inline uint32_t ring_free(RogueThreadPool *tp){ uint32_t h=(uint32_t)SDL_AtomicGet(&tp->head); uint32_t t=(uint32_t)SDL_AtomicGet(&tp->tail); return ROGUE_TP_RING - (h - t); }

int rogue_thread_pool_init(RogueThreadPool *tp, int threads){ if(!tp) return -1; if(threads<=0) threads=1; memset(tp,0,sizeof *tp); tp->thread_count=threads; tp->threads=(SDL_Thread**)calloc((size_t)threads,sizeof(SDL_Thread*)); if(!tp->threads) return -1; if(rogue_sem_init(&tp->work_sem,0)!=0) { free(tp->threads); return -1; } SDL_AtomicSet(&tp->head,0); SDL_AtomicSet(&tp->tail,0); SDL_AtomicSet(&tp->tasks_submitted,0); SDL_AtomicSet(&tp->tasks_executed,0); SDL_AtomicSet(&tp->worker_wakeups,0); SDL_AtomicSet(&tp->peak_queue,0); tp->debug_yield=0; tp->priority=SDL_THREAD_PRIORITY_NORMAL; tp->running=1; for(int i=0;i<threads;i++){ char name[32]; snprintf(name,sizeof name,"tpw-%d",i); tp->threads[i]=SDL_CreateThread(worker_main,name,tp); if(!tp->threads[i]){ tp->running=0; // cleanup
            for(int j=0;j<i;j++){ rogue_sem_post(&tp->work_sem); }
            for(int j=0;j<i;j++){ SDL_WaitThread(tp->threads[j],NULL); }
            rogue_sem_destroy(&tp->work_sem); free(tp->threads); return -1; } }
    return 0; }

void rogue_thread_pool_shutdown(RogueThreadPool *tp){ if(!tp||!tp->threads) return; tp->running=0; // wake all
    for(int i=0;i<tp->thread_count;i++){ rogue_sem_post(&tp->work_sem); }
    for(int i=0;i<tp->thread_count;i++){ if(tp->threads[i]) SDL_WaitThread(tp->threads[i],NULL); }
    rogue_sem_destroy(&tp->work_sem); free(tp->threads); tp->threads=NULL; }

int rogue_thread_pool_submit(RogueThreadPool *tp, RogueTaskFn fn, void *user){ if(!tp||!fn) return -1; uint32_t free = ring_free(tp); if(free==0) return -2; uint32_t h=(uint32_t)SDL_AtomicGet(&tp->head); tp->ring[h & (ROGUE_TP_RING-1)] = (RogueTask){fn,user}; SDL_AtomicSet(&tp->head,(int)(h+1)); SDL_AtomicAdd(&tp->tasks_submitted,1); // update peak
    uint32_t pend = (uint32_t)rogue_thread_pool_pending(tp);
    uint32_t peak = (uint32_t)SDL_AtomicGet(&tp->peak_queue);
    while(pend>peak && !SDL_AtomicCAS(&tp->peak_queue,(int)peak,(int)pend)) { peak = (uint32_t)SDL_AtomicGet(&tp->peak_queue); }
    rogue_sem_post(&tp->work_sem); return 0; }

int rogue_thread_pool_pending(RogueThreadPool *tp){ if(!tp) return 0; uint32_t h=(uint32_t)SDL_AtomicGet(&tp->head); uint32_t t=(uint32_t)SDL_AtomicGet(&tp->tail); return (int)(h - t); }

void rogue_thread_pool_get_stats(RogueThreadPool *tp, RogueThreadPoolStats *out){ if(!tp||!out) return; out->pending=(uint32_t)rogue_thread_pool_pending(tp); out->peak_queue=(uint32_t)SDL_AtomicGet(&tp->peak_queue); out->worker_wakeups=(uint32_t)SDL_AtomicGet(&tp->worker_wakeups); out->tasks_submitted=(uint32_t)SDL_AtomicGet(&tp->tasks_submitted); out->tasks_executed=(uint32_t)SDL_AtomicGet(&tp->tasks_executed); }

void rogue_thread_pool_set_priority(RogueThreadPool *tp, SDL_ThreadPriority pri){ if(!tp) return; tp->priority=pri;
}

void rogue_thread_pool_set_debug_yield(RogueThreadPool *tp, bool enable){ if(!tp) return; tp->debug_yield = enable?1:0; }
