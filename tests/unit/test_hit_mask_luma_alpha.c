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

static SDL_Surface* make_checker_no_alpha(int w, int h)
{
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, SDL_PIXELFORMAT_RGB24);
    assert(surf);
    uint8_t* pixels = (uint8_t*) surf->pixels;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int on = ((x ^ y) & 1);
            uint8_t v = on ? 255 : 0;
            pixels[(y * w + x) * 3 + 0] = v;
            pixels[(y * w + x) * 3 + 1] = v;
            pixels[(y * w + x) * 3 + 2] = v;
        }
    }
    return surf;
}

static void count_bits(const RogueHitPixelMaskFrame* f, int* out_count)
{
    int c = 0;
    for (int y = 0; y < f->height; ++y)
        for (int x = 0; x < f->width; ++x)
            c += rogue_hit_mask_test(f, x, y) ? 1 : 0;
    *out_count = c;
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const int W = 32, H = 32;
    SDL_Surface* surf = make_checker_no_alpha(W, H);

    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.compression_level = 0;
    cfg.mipmap_levels = 1;
    cfg.alpha_threshold = 0.5f;

    /* Without derive_alpha_from_luma, no alpha channel => fully solid (default opaque). */
    RogueHitPixelMaskFrame f0 = {0};
    RoguePixelMaskMetrics m0 = {0};
    int ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &f0, &m0);
    assert(ok);
    int total = W * H;
    if (!(m0.collision_pixels == (uint32_t) total))
    {
        fprintf(stderr, "expected fully solid without luma alpha: %u vs %d\n", m0.collision_pixels,
                total);
        return 2;
    }

    /* With derive_alpha_from_luma, checker should produce ~50% solids at threshold 0.5 */
    RogueHitPixelMaskFrame f1 = {0};
    RoguePixelMaskMetrics m1 = {0};
    cfg.derive_alpha_from_luma = 1;
    ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &f1, &m1);
    assert(ok);
    if (!(m1.collision_pixels > 0 && m1.collision_pixels < (uint32_t) total))
    {
        fprintf(stderr, "expected partial solids with luma alpha: got %u of %d\n",
                m1.collision_pixels, total);
        return 3;
    }

    free(f0.bits);
    free(f1.bits);
    SDL_FreeSurface(surf);
    SDL_Quit();
    printf("hit_mask_luma_alpha: PASS\n");
    return 0;
}
#endif
