#include "../../src/core/integration/thread_pool.h"
#include "../../src/game/hit_pixel_mask.h"
#include "../../src/game/pixel_mask_loader.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef main
#undef main
#endif

/* Minimal fake surface struct subset used by loader when SDL not present. If SDL is present the
 * real headers will be used instead (ROGUE_HAVE_SDL defined in test cmake). For simplicity we
 * require SDL for this test; skip if not available. */

#if !defined(ROGUE_HAVE_SDL)
int main(void)
{
    printf("skip (no SDL)\n");
    return 0;
}
#else
#include <SDL.h>
#include <SDL_image.h>

static SDL_Surface* make_test_surface(int w, int h)
{
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    assert(surf);
    /* Draw a filled 8x8 square in center */
    uint32_t* pixels = (uint32_t*) surf->pixels;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int inside = (x >= 4 && x < 12 && y >= 4 && y < 12);
            uint8_t a = inside ? 255 : 0;
            pixels[y * w + x] = (a << 24) | 0x00FFFFFF; /* white rgb */
        }
    }
    return surf;
}

static void verify_distance_field(const RogueHitPixelMaskFrame* f)
{
    assert(f->distance_field && f->distance_field_scale == 10);
    /* Center point (8,8) should be positive (inside) and larger than a boundary point */
    int w = f->width;
    int16_t center = f->distance_field[8 + 8 * w];
    int16_t boundary = f->distance_field[4 + 4 * w];
    assert(center > 0);
    assert(boundary == 0 || boundary < center);
    /* Outside point (0,0) negative */
    int16_t outside = f->distance_field[0];
    assert(outside < 0);
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.generate_distance_fields = 1;

    SDL_Surface* surf = make_test_surface(16, 16);
    RogueHitPixelMaskFrame frame = {0};
    RoguePixelMaskMetrics metrics = {0};
    int ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &frame, &metrics);
    assert(ok);
    verify_distance_field(&frame);
    SDL_FreeSurface(surf);
    free(frame.bits);
    free(frame.distance_field);

    /* Async path */
    SDL_Surface* surf2 = make_test_surface(16, 16);
    RogueThreadPool tp = {0};
    assert(rogue_thread_pool_init(&tp, 2) == 0);
    rogue_pixel_mask_set_thread_pool(&tp);
    RogueHitPixelMaskFrame frame_async = {0};
    RoguePixelMaskMetrics metrics_async = {0};
    ok = rogue_pixel_mask_build_async(surf2, &cfg, &frame_async, &metrics_async, 1);
    assert(ok);
    /* Poll until distance field populated */
    int spins = 0;
    while (!frame_async.distance_field && spins < 1000)
    {
        SDL_Delay(1);
        spins++;
    }
    assert(frame_async.distance_field);
    verify_distance_field(&frame_async);
    free(frame_async.bits);
    free(frame_async.distance_field);
    rogue_thread_pool_shutdown(&tp);
    printf("hit_mask_distance_field: PASS\n");
    SDL_Quit();
    return 0;
}
#endif
