#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <SDL.h>
#include "../../src/core/integration/threading.h"

typedef struct { RogueBarrier *bar; SDL_atomic_t *ok; } Ctx;
static int thread_main(void *ud){ Ctx *c = (Ctx*)ud; uint32_t g = rogue_barrier_wait(c->bar); if(g==1) SDL_AtomicSet(c->ok,1); return 0; }

int main(void){ if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){ fprintf(stderr,"SDL init failed: %s\n", SDL_GetError()); return 2; }
    RogueBarrier b; if(rogue_barrier_init(&b,3)!=0){ fprintf(stderr,"barrier init failed\n"); return 2; }
    SDL_atomic_t ok; SDL_AtomicSet(&ok,0);
    Ctx c={&b,&ok}; SDL_Thread *t1=SDL_CreateThread(thread_main,"b1",&c); SDL_Thread *t2=SDL_CreateThread(thread_main,"b2",&c);
    SDL_Delay(10);
    uint32_t g0 = rogue_barrier_wait(&b); // should release all 3
    SDL_WaitThread(t1,NULL); SDL_WaitThread(t2,NULL);
    rogue_barrier_destroy(&b); SDL_Quit();
    int val = SDL_AtomicGet(&ok);
    if(g0!=1 || val!=1){ fprintf(stderr,"barrier failed g0=%u ok=%d\n",g0,val); return 1; }
    printf("BARRIER_OK\n");
    return 0;
}
