#include "../../src/game/hit_pixel_mask.h"
#include "../../src/game/pixel_mask_loader.h"
#include <assert.h>
#include <stdio.h>

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

static SDL_Surface* make_rgba_alpha_ramp(int w, int h)
{
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    assert(surf);
    uint32_t* pixels = (uint32_t*) surf->pixels;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            uint8_t a = (uint8_t) ((x * 255) / (w - 1));
            pixels[y * w + x] = (a << 24) | 0x00000000;
        }
    }
    return surf;
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const int W = 256, H = 8;
    SDL_Surface* surf = make_rgba_alpha_ramp(W, H);

    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.compression_level = 0;
    cfg.mipmap_levels = 1;

    RogueHitPixelMaskFrame f1 = {0};
    RoguePixelMaskMetrics m1 = {0};
    cfg.alpha_threshold = 0.5f;
    cfg.alpha_gamma = 1.0f; /* no gamma */
    int ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &f1, &m1);
    assert(ok);

    RogueHitPixelMaskFrame f2 = {0};
    RoguePixelMaskMetrics m2 = {0};
    cfg.alpha_threshold = 0.5f;
    cfg.alpha_gamma = 2.0f; /* darker after gamma => fewer solids */
    ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &f2, &m2);
    assert(ok);

    /* Expect strictly fewer or equal collision pixels when gamma > 1 (compressing lower alphas) */
    if (!(m2.collision_pixels <= m1.collision_pixels))
    {
        fprintf(stderr, "alpha_gamma expectation failed: %u !<= %u\n", m2.collision_pixels,
                m1.collision_pixels);
        return 2;
    }

    free(f1.bits);
    free(f2.bits);
    SDL_FreeSurface(surf);
    SDL_Quit();
    printf("hit_mask_alpha_gamma: PASS\n");
    return 0;
}
#endif
