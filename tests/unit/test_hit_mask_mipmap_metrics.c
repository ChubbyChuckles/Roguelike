#include "game/hit_pixel_mask.h"
#include "game/pixel_mask_loader.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

static SDL_Surface* make_checker(int w, int h, int cell)
{
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s)
        return NULL;
    SDL_LockSurface(s);
    for (int y = 0; y < h; ++y)
    {
        Uint32* row = (Uint32*) ((Uint8*) s->pixels + y * s->pitch);
        for (int x = 0; x < w; ++x)
        {
            int on = ((x / cell) ^ (y / cell)) & 1;
            Uint8 a = on ? 255 : 0;
            row[x] = (Uint32) (0x00FFFFFF | (a << 24));
        }
    }
    SDL_UnlockSurface(s);
    return s;
}

int main(void)
{
#if !defined(ROGUE_HAVE_SDL)
    printf("SKIP (no SDL)\n");
    return 0;
#else
    printf("DEBUG: entering main...\n");
    fflush(stdout);
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("DEBUG: SDL_Init ok\n");
    fflush(stdout);
    SDL_Surface* s = make_checker(32, 32, 2);
    if (!s)
    {
        printf("surface alloc failed\n");
        fflush(stdout);
        return 1;
    }
    printf("DEBUG: surface ok\n");
    fflush(stdout);
    RogueHitPixelMaskFrame f = {0};
    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.mipmap_levels = 4;
    RoguePixelMaskMetrics m = {0};
    /* allocate per-mip arrays (levels == 4 => indices 0..3) */
    uint32_t mip_total[8] = {0};
    uint32_t mip_coll[8] = {0};
    float mip_ratio[8] = {0};
    m.mip_total_pixels = mip_total;
    m.mip_collision_pixels = mip_coll;
    m.mip_solid_ratio = mip_ratio;
    printf("DEBUG: calling build...\n");
    int ok = rogue_pixel_mask_build_from_surface(s, &cfg, &f, &m);
    printf("DEBUG: build returned %d\n", ok);
    SDL_FreeSurface(s);
    if (!ok)
    {
        printf("build failed\n");
        return 1;
    }

    if (m.mipmap_levels != 4)
    {
        printf("expected 4 mip levels, got %d\n", m.mipmap_levels);
        return 1;
    }
    /* Base */
    if (m.mip_total_pixels[0] != (uint32_t) (f.width * f.height))
    {
        printf("base total mismatch\n");
        return 1;
    }
    if (m.mip_collision_pixels[0] != m.collision_pixels)
    {
        printf("base collision mismatch\n");
        return 1;
    }
    if (m.mip_solid_ratio[0] != m.solid_ratio)
    {
        printf("base ratio mismatch\n");
        return 1;
    }
    /* Monotonic non-increasing occupancy */
    for (int i = 1; i < 4; ++i)
    {
        if (m.mip_collision_pixels[i] > m.mip_collision_pixels[i - 1])
        {
            printf("mip occupancy increased at level %d: %u > %u\n", i, m.mip_collision_pixels[i],
                   m.mip_collision_pixels[i - 1]);
            return 1;
        }
    }
    if (!m.mip_conservative_monotonic)
    {
        printf("monotonic flag not set\n");
        return 1;
    }
    /* spot-check ratios within [0,1] */
    for (int i = 0; i < 4; ++i)
    {
        if (m.mip_solid_ratio[i] < 0.0f || m.mip_solid_ratio[i] > 1.0f)
        {
            printf("ratio out of range at level %d: %f\n", i, m.mip_solid_ratio[i]);
            return 1;
        }
    }
    /* cleanup */
    printf("DEBUG: entering frame reset...\n");
    rogue_hit_mask_frame_reset(&f);
    SDL_Quit();
    return 0;
#endif
}
