#define SDL_MAIN_HANDLED
#include "../../src/core/integration/thread_pool.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_atomic_t s_sum;
static SDL_atomic_t s_left;

static void work(void* u)
{
    int v = (int) (intptr_t) u;
    SDL_AtomicAdd(&s_sum, v);
    SDL_AtomicAdd(&s_left, -1);
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 2;
    }
    RogueThreadPool tp;
    if (rogue_thread_pool_init(&tp, 4) != 0)
    {
        fprintf(stderr, "tp init failed\n");
        return 2;
    }
    SDL_AtomicSet(&s_sum, 0);
    int N = 1000;
    SDL_AtomicSet(&s_left, N);
    for (int i = 1; i <= N; i++)
    {
        if (rogue_thread_pool_submit(&tp, work, (void*) (intptr_t) 1) != 0)
        {
            fprintf(stderr, "submit failed\n");
            return 2;
        }
    }
    // wait until countdown hits zero or timeout
    uint32_t start = SDL_GetTicks();
    while (SDL_AtomicGet(&s_left) > 0)
    {
        if (SDL_GetTicks() - start > 2000)
        {
            break;
        }
        SDL_Delay(1);
    }
    int val = SDL_AtomicGet(&s_sum);
    rogue_thread_pool_shutdown(&tp);
    SDL_Quit();
    if (val != N)
    {
        fprintf(stderr, "sum %d != %d (left=%d)\n", val, N, SDL_AtomicGet(&s_left));
        return 1;
    }
    printf("THREAD_POOL_OK\n");
    return 0;
}
