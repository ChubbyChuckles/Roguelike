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

static SDL_Surface* make_speckled_pattern(int w, int h)
{
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    assert(surf);
    uint32_t* p = (uint32_t*) surf->pixels;
    memset(p, 0, (size_t) w * (size_t) h * sizeof(uint32_t));
    /* Draw a filled box */
    for (int y = 8; y < h - 8; ++y)
        for (int x = 8; x < w - 8; ++x)
            p[y * w + x] = 0xFF000000u; /* alpha=255 */
    /* Add isolated single pixels near edges */
    p[2 * w + 2] = 0xFF000000u;
    p[(h - 3) * w + (w - 3)] = 0xFF000000u;
    return surf;
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const int W = 48, H = 40;
    SDL_Surface* surf = make_speckled_pattern(W, H);

    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.compression_level = 0;
    cfg.mipmap_levels = 1;
    cfg.alpha_threshold = 0.5f;

    RogueHitPixelMaskFrame f0 = {0};
    RoguePixelMaskMetrics m0 = {0};
    int ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &f0, &m0);
    assert(ok);

    RogueHitPixelMaskFrame f1 = {0};
    RoguePixelMaskMetrics m1 = {0};
    cfg.edge_smoothing_passes = 1;
    ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &f1, &m1);
    assert(ok);

    /* Expect fewer solids after removing isolated speckles, but not zero and not bigger */
    if (!(m1.collision_pixels < m0.collision_pixels && m1.collision_pixels > 0))
    {
        fprintf(stderr, "smoothing expectation failed: %u !< %u\n", m1.collision_pixels,
                m0.collision_pixels);
        return 2;
    }

    free(f0.bits);
    free(f1.bits);
    SDL_FreeSurface(surf);
    SDL_Quit();
    printf("hit_mask_smoothing: PASS\n");
    return 0;
}
#endif
