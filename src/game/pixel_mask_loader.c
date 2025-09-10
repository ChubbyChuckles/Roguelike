/* pixel_mask_loader.c - Phase 1 (Milestone 1.1) minimal implementation
 * Converts an SDL surface (or image file) into a RogueHitPixelMaskFrame.
 * Scope purposely small for initial integration:
 *  - Alpha thresholding only
 *  - No compression/mipmaps/SDF yet
 *  - Headless safe (returns 0 if SDL not compiled)
 */
#include "pixel_mask_loader.h"
#include "../util/log.h"
#include "hit_pixel_mask.h" /* for RogueHitPixelMaskFrame helpers */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(ROGUE_HAVE_SDL)
#include <SDL.h>
#endif
#if defined(ROGUE_HAVE_SDL_IMAGE)
#include <SDL_image.h>
#endif

static uint64_t now_ns(void)
{
#if defined(_WIN32)
    LARGE_INTEGER freq, ctr;
    if (!QueryPerformanceFrequency(&freq) || !QueryPerformanceCounter(&ctr))
        return 0;
    return (uint64_t) ((ctr.QuadPart * 1000000000ULL) / (uint64_t) freq.QuadPart);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;
#endif
}

int rogue_pixel_mask_build_from_surface(void* sdl_surface_v, const RoguePixelMaskLoadConfig* cfg,
                                        struct RogueHitPixelMaskFrame* out_frame,
                                        RoguePixelMaskMetrics* out_metrics)
{
    if (out_metrics)
    {
        memset(out_metrics, 0, sizeof(*out_metrics));
    }
    if (!out_frame)
        return 0;
    if (!cfg)
    {
        static RoguePixelMaskLoadConfig def;
        def = rogue_pixel_mask_load_config_default();
        cfg = &def;
    }
#if !defined(ROGUE_HAVE_SDL)
    (void) sdl_surface_v;
    (void) cfg;
    return 0;
#else
    SDL_Surface* surf = (SDL_Surface*) sdl_surface_v;
    if (!surf)
        return 0;
    if (SDL_LockSurface(surf) != 0)
    {
        return 0;
    }
    int w = surf->w, h = surf->h;
    /* allocate frame */
    out_frame->width = w;
    out_frame->height = h;
    out_frame->origin_x = 0;
    out_frame->origin_y = 0;
    out_frame->pitch_words = (w + 31) / 32;
    size_t words = (size_t) out_frame->pitch_words * (size_t) h;
    out_frame->bits = (uint32_t*) malloc(words * sizeof(uint32_t));
    if (!out_frame->bits)
    {
        SDL_UnlockSurface(surf);
        return 0;
    }
    memset(out_frame->bits, 0, words * sizeof(uint32_t));
    uint64_t t0 = now_ns();
    const float thresh = (cfg->alpha_threshold <= 0.f)
                             ? 0.f
                             : (cfg->alpha_threshold >= 1.f ? 1.f : cfg->alpha_threshold);
    int bpp = surf->format->BytesPerPixel;
    const unsigned char* base = (const unsigned char*) surf->pixels;
    for (int y = 0; y < h; y++)
    {
        const unsigned char* row = base + (size_t) y * surf->pitch;
        for (int x = 0; x < w; x++)
        {
            const unsigned char* px = row + x * bpp;
            unsigned a = 255; /* default opaque */
            if (surf->format->Amask)
            {
                Uint32 pix = 0;
                switch (bpp)
                {
                case 1:
                    pix = *px;
                    break;
                case 2:
                    pix = *(const uint16_t*) px;
                    break;
                case 3:
                    if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                        pix = (px[0] << 16) | (px[1] << 8) | px[2];
                    else
                        pix = px[0] | (px[1] << 8) | (px[2] << 16);
                    break;
                case 4:
                    pix = *(const uint32_t*) px;
                    break;
                }
                Uint8 r, g, b, aa;
                SDL_GetRGBA(pix, surf->format, &r, &g, &b, &aa);
                a = aa;
            }
            if (a >= (unsigned) (thresh * 255.0f + 0.5f))
            {
                rogue_hit_mask_set(out_frame, x, y);
                if (out_metrics)
                    out_metrics->collision_pixels++;
            }
            if (out_metrics)
                out_metrics->total_pixels++;
        }
    }
    SDL_UnlockSurface(surf);
    if (out_metrics)
    {
        uint64_t t1 = now_ns();
        out_metrics->build_time_ns = (t0 && t1) ? (t1 - t0) : 0;
        out_metrics->solid_ratio =
            out_metrics->total_pixels
                ? (float) out_metrics->collision_pixels / (float) out_metrics->total_pixels
                : 0.f;
        out_metrics->memory_footprint = words * sizeof(uint32_t);
    }
    return 1;
#endif
}

int rogue_pixel_mask_load_from_file(const char* path, const RoguePixelMaskLoadConfig* cfg,
                                    struct RogueHitPixelMaskFrame* out_frame,
                                    RoguePixelMaskMetrics* out_metrics)
{
    if (out_metrics)
        memset(out_metrics, 0, sizeof(*out_metrics));
    if (!path || !out_frame)
        return 0;
#if !defined(ROGUE_HAVE_SDL)
    (void) path;
    (void) cfg;
    return 0;
#else
    SDL_Surface* surf = NULL;
#if defined(ROGUE_HAVE_SDL_IMAGE)
    surf = IMG_Load(path);
#endif
    if (!surf)
    {
        ROGUE_LOG_DEBUG("pixel_mask_load_fallback_placeholder: %s", path);
        return 0; /* caller may fall back to placeholder */
    }
    int ok = rogue_pixel_mask_build_from_surface(surf, cfg, out_frame, out_metrics);
    SDL_FreeSurface(surf);
    return ok;
#endif
}
