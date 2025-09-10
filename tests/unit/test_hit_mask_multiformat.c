/* test_hit_mask_multiformat.c
 * Validates multi-format loading fallback (BMP) for pixel mask loader.
 * Creates a small in-memory SDL surface, saves to BMP, then loads via
 * rogue_pixel_mask_load_from_file and ensures collision bits match expectation.
 */
#include "game/hit_pixel_mask.h"
#include "game/pixel_mask_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ROGUE_HAVE_SDL)
#include <SDL.h>
#include <SDL_image.h>
#endif

static int write_test_bmp(const char* path)
{
#if !defined(ROGUE_HAVE_SDL)
    (void) path;
    return 0;
#else
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 8, 8, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf)
        return 0;
    if (SDL_LockSurface(surf) != 0)
    {
        SDL_FreeSurface(surf);
        return 0;
    }
    /* Draw a simple plus shape (center row & column opaque) */
    for (int y = 0; y < 8; ++y)
    {
        Uint32* row = (Uint32*) ((unsigned char*) surf->pixels + y * surf->pitch);
        for (int x = 0; x < 8; ++x)
        {
            int solid = (x == 4) || (y == 4);
            Uint8 a = solid ? 255 : 0;
            row[x] = (a << 24) | 0x00FFFFFFu;
        }
    }
    SDL_UnlockSurface(surf);
    int ok = SDL_SaveBMP(surf, path) == 0;
    SDL_FreeSurface(surf);
    return ok;
#endif
}

int main(void)
{
#if !defined(ROGUE_HAVE_SDL)
    /* Nothing to do headless */
    return 0;
#else
    const char* path = "build/test_multiformat_tmp.bmp";
    if (!write_test_bmp(path))
    {
        fprintf(stderr, "Failed to write test BMP\n");
        return 1;
    }
    RogueHitPixelMaskFrame frame;
    memset(&frame, 0, sizeof(frame));
    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.compression_level = 0;
    cfg.mipmap_levels = 1;
    RoguePixelMaskMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    int ok = rogue_pixel_mask_load_from_file(path, &cfg, &frame, &metrics);
    if (!ok)
    {
        fprintf(stderr, "rogue_pixel_mask_load_from_file failed\n");
        return 1;
    }
    /* Expect 15 solid pixels (8 vertical + 8 horizontal - 1 center overlap) */
    int count = 0;
    for (int y = 0; y < frame.height; ++y)
        for (int x = 0; x < frame.width; ++x)
            if (rogue_hit_mask_test(&frame, x, y))
                count++;
    if (count != 15)
    {
        fprintf(stderr, "Unexpected solid pixel count %d (expected 15)\n", count);
        return 1;
    }
    /* Basic metrics sanity */
    if (metrics.collision_pixels != (uint32_t) count || metrics.total_pixels != 64)
    {
        fprintf(stderr, "Metrics mismatch (collision=%u total=%u)\n", metrics.collision_pixels,
                metrics.total_pixels);
        return 1;
    }
    free(frame.bits);
    return 0;
#endif
}
