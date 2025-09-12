#include "../../src/core/integration/thread_pool.h"
#include "../../src/game/hit_pixel_mask.h"
#include "../../src/game/pixel_mask_loader.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef main
#undef main
#endif

#if !defined(ROGUE_HAVE_SDL)
int main(void)
{
    printf("skip (no SDL)\n");
    return 0;
}
#else
#include <SDL.h>

static SDL_Surface* make_test_surface(int w, int h)
{
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    assert(surf);
    /* Draw a gradient alpha pattern with deterministic thresholding */
    uint32_t* pixels = (uint32_t*) surf->pixels;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            /* alpha ramps horizontally */
            uint8_t a = (uint8_t) ((x * 255) / (w - 1));
            pixels[y * w + x] = (a << 24) | 0x00223344;
        }
    }
    return surf;
}

static int compare_frames(const RogueHitPixelMaskFrame* a, const RogueHitPixelMaskFrame* b)
{
    if (a->width != b->width || a->height != b->height || a->pitch_words != b->pitch_words)
        return 0;
    size_t words = (size_t) a->pitch_words * (size_t) a->height;
    return memcmp(a->bits, b->bits, words * sizeof(uint32_t)) == 0;
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.alpha_threshold = 0.5f;
    cfg.mipmap_levels = 1;
    cfg.compression_level = 0;

    SDL_Surface* surf = make_test_surface(257, 193);

    RogueHitPixelMaskFrame single = {0};
    RoguePixelMaskMetrics m1 = {0};
    int ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &single, &m1);
    assert(ok);

    /* Threaded build using shared thread pool */
    RogueThreadPool tp = {0};
    assert(rogue_thread_pool_init(&tp, 3) == 0);
    rogue_pixel_mask_set_thread_pool(&tp);
    RogueHitPixelMaskFrame threaded = {0};
    RoguePixelMaskMetrics m2 = {0};
    ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &threaded, &m2);
    assert(ok);

    /* Parity check */
    if (!compare_frames(&single, &threaded))
    {
        fprintf(stderr, "threaded parity mismatch: bit patterns differ\n");
        rogue_thread_pool_shutdown(&tp);
        SDL_FreeSurface(surf);
        return 2;
    }

    /* Metrics sanity: totals equal; collision count equal */
    assert(m1.total_pixels == m2.total_pixels);
    assert(m1.collision_pixels == m2.collision_pixels);

    free(single.bits);
    free(threaded.bits);
    rogue_thread_pool_shutdown(&tp);
    SDL_FreeSurface(surf);
    SDL_Quit();
    printf("threaded_parity_ok\n");
    return 0;
}
#endif
