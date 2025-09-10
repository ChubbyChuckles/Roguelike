/* pixel_mask_loader.c - Phase 1 (Milestone 1.1+) incremental implementation
 * Converts an SDL surface (or image file) into a RogueHitPixelMaskFrame.
 * Current capabilities:
 *  - Alpha thresholding
 *  - Optional simple RLE compression of the base bit buffer (row-major)
 *  - Optional mipmap generation (binary downscale OR of 2x2 blocks)
 *  - Metrics collection (timings, counts, memory usage, compressed size, mip levels)
 * Future (deferred): edge smoothing, distance fields, threaded build, advanced codecs.
 */
#include "pixel_mask_loader.h"
#include "../util/log.h"
#include "hit_pixel_mask.h" /* for RogueHitPixelMaskFrame helpers */
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#endif

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

/* Simple run-length encoding of 32-bit word stream. Format:
 * [u32 run_length][u32 value] repeated until all words emitted.
 * Returns allocated buffer + size via out_size. Caller frees. */
static void* rle_compress_words(const uint32_t* words, size_t count, size_t* out_size)
{
    if (out_size)
        *out_size = 0;
    if (!words || count == 0)
        return NULL;
    /* Worst-case expansion: every word different -> 2 u32 per input word */
    size_t cap = count * 2 * sizeof(uint32_t);
    uint32_t* buf = (uint32_t*) malloc(cap);
    if (!buf)
        return NULL;
    size_t w = 0;
    size_t i = 0;
    while (i < count)
    {
        uint32_t v = words[i];
        size_t run = 1;
        while (i + run < count && words[i + run] == v && run < UINT32_MAX)
            run++;
        buf[w++] = (uint32_t) run;
        buf[w++] = v;
        i += run;
    }
    size_t bytes = w * sizeof(uint32_t);
    /* Optionally shrink (realloc) */
    uint32_t* shrunk = (uint32_t*) realloc(buf, bytes);
    if (shrunk)
        buf = shrunk;
    if (out_size)
        *out_size = bytes;
    return buf;
}

/* Generate binary mipmaps (bit OR of 2x2 parent texels). Allocates mipmaps array inside frame. */
static int generate_mipmaps(struct RogueHitPixelMaskFrame* frame, int requested_levels,
                            RoguePixelMaskMetrics* metrics)
{
    if (!frame || requested_levels <= 1)
    {
        if (frame)
        {
            frame->mipmap_count = 1;
            frame->mipmaps = NULL;
        }
        if (metrics)
            metrics->mipmap_levels = 1;
        return 1;
    }
    if (requested_levels > 6)
        requested_levels = 6;
    frame->mipmap_count = 1; /* base */
    frame->mipmaps = NULL;
    int current_w = frame->width;
    int current_h = frame->height;
    int max_levels = 1;
    while (current_w > 1 || current_h > 1)
    {
        current_w = current_w > 1 ? (current_w + 1) / 2 : 1;
        current_h = current_h > 1 ? (current_h + 1) / 2 : 1;
        max_levels++;
        if (max_levels >= requested_levels)
            break;
    }
    if (max_levels <= 1)
    {
        if (metrics)
            metrics->mipmap_levels = 1;
        return 1;
    }
    frame->mipmaps = (struct RogueHitPixelMaskMipmapLevel*) calloc((size_t) (max_levels - 1),
                                                                   sizeof(*frame->mipmaps));
    if (!frame->mipmaps)
        return 0;
    frame->mipmap_count = max_levels;
    /* Build chain */
    const uint32_t* prev_bits = frame->bits;
    int prev_w = frame->width;
    int prev_h = frame->height;
    int prev_pitch_words = frame->pitch_words;
    for (int level = 1; level < max_levels; ++level)
    {
        struct RogueHitPixelMaskMipmapLevel* ml = &frame->mipmaps[level - 1];
        ml->width = prev_w > 1 ? (prev_w + 1) / 2 : 1;
        ml->height = prev_h > 1 ? (prev_h + 1) / 2 : 1;
        ml->pitch_words = (ml->width + 31) / 32;
        size_t words = (size_t) ml->pitch_words * (size_t) ml->height;
        ml->bits = (uint32_t*) calloc(words, sizeof(uint32_t));
        if (!ml->bits)
            return 0;
        for (int y = 0; y < ml->height; ++y)
        {
            for (int x = 0; x < ml->width; ++x)
            {
                /* OR 2x2 block from parent */
                int src_x = x * 2;
                int src_y = y * 2;
                int hit = 0;
                for (int oy = 0; oy < 2 && (src_y + oy) < prev_h; ++oy)
                {
                    for (int ox = 0; ox < 2 && (src_x + ox) < prev_w; ++ox)
                    {
                        int idx = (src_y + oy) * prev_pitch_words + ((src_x + ox) >> 5);
                        uint32_t m = 1u << ((src_x + ox) & 31);
                        if (prev_bits[idx] & m)
                        {
                            hit = 1;
                            goto done_block;
                        }
                    }
                }
            done_block:
                if (hit)
                {
                    int dst_idx = y * ml->pitch_words + (x >> 5);
                    ml->bits[dst_idx] |= (1u << (x & 31));
                }
            }
        }
        prev_bits = ml->bits;
        prev_w = ml->width;
        prev_h = ml->height;
        prev_pitch_words = ml->pitch_words;
    }
    if (metrics)
        metrics->mipmap_levels = max_levels;
    return 1;
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
    out_frame->mipmaps = NULL;
    out_frame->mipmap_count = 1;
    out_frame->compressed = NULL;
    out_frame->compressed_size = 0;
    out_frame->compressed_format = 0;
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
    /* Compression (RLE) */
    if (cfg->compression_level > 0)
    {
        out_frame->compressed =
            rle_compress_words(out_frame->bits, words, &out_frame->compressed_size);
        if (out_frame->compressed)
            out_frame->compressed_format = 1; /* RLE */
        if (out_metrics)
            out_metrics->compressed_size = out_frame->compressed ? out_frame->compressed_size : 0;
    }
    /* Mipmaps */
    if (cfg->mipmap_levels > 1)
    {
        if (!generate_mipmaps(out_frame, cfg->mipmap_levels, out_metrics))
        {
            /* leave base valid; on failure just skip advanced features */
            if (out_metrics)
                out_metrics->mipmap_levels = 1;
        }
    }
    else if (out_metrics)
    {
        out_metrics->mipmap_levels = 1;
    }
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
