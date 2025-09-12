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
#include "../core/integration/thread_pool.h"
#include "../util/log.h"
#include "hit_pixel_mask.h" /* for RogueHitPixelMaskFrame helpers */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(_MSC_VER)
#pragma warning(push)
/* Suppress constant conditional (C4127) for deliberate while/if patterns in tight loops */
#pragma warning(disable : 4127)
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

/* Optional thread pool registration. We avoid requiring a global symbol definition to keep
   this module link-order agnostic. Call rogue_pixel_mask_set_thread_pool(tp) during engine
   init to enable asynchronous builds. */
static RogueThreadPool* g_registered_thread_pool = NULL;

void rogue_pixel_mask_set_thread_pool(RogueThreadPool* tp) { g_registered_thread_pool = tp; }

/* ------------------------------------------------------------------------------------------------
   Deterministic stripe-based parallelization for mask build
   We partition rows into contiguous stripes and submit them to the shared thread pool when
   available. Each job writes to disjoint rows, so no synchronization on the output buffer is
   required. Completion is coordinated via a semaphore (one post per finished stripe).
   Metrics (collision/total) are reduced on the main thread in a fixed order.
*/
typedef struct StripeJob
{
    /* Input surface information */
    const unsigned char* base;
    int pitch;
    int bpp;
    SDL_PixelFormat* fmt;
    int w;
    int h;
    int y0;
    int y1; /* exclusive */
    float thresh;
    float alpha_gamma;          /* gamma to apply to normalized alpha */
    int derive_alpha_from_luma; /* if no alpha channel, derive alpha from luma */
    /* Output */
    uint32_t* out_bits;
    int out_pitch_words;
    /* Local counters */
    uint32_t collision_pixels;
    uint32_t total_pixels;
    /* Completion signaling */
    RogueSem* done_sem;
} StripeJob;

static void stripe_job_run(StripeJob* j)
{
    const int w = j->w;
    const int y0 = j->y0;
    const int y1 = j->y1;
    const float thr = j->thresh;
    const float gamma = (j->alpha_gamma > 0.0f) ? j->alpha_gamma : 1.0f;
    const unsigned char* base = j->base;
    const int pitch = j->pitch;
    const int bpp = j->bpp;
    SDL_PixelFormat* fmt = j->fmt;
    uint32_t* out_bits = j->out_bits;
    const int pitch_words = j->out_pitch_words;
    uint32_t coll = 0, tot = 0;

    for (int y = y0; y < y1; ++y)
    {
        const unsigned char* row = base + (size_t) y * (size_t) pitch;
        uint32_t* dst_row = out_bits + (size_t) y * (size_t) pitch_words;
        for (int x = 0; x < w; ++x)
        {
            const unsigned char* px = row + (size_t) x * (size_t) bpp;
            float an = 1.0f; /* normalized alpha */
            if (fmt->Amask)
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
                SDL_GetRGBA(pix, fmt, &r, &g, &b, &aa);
                an = (float) aa * (1.0f / 255.0f);
            }
            else if (j->derive_alpha_from_luma)
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
                SDL_GetRGBA(pix, fmt, &r, &g, &b, &aa);
                /* Rec.709 luma approximation on 0..255 range */
                float Y = 0.2126f * (float) r + 0.7152f * (float) g + 0.0722f * (float) b;
                an = Y * (1.0f / 255.0f);
            }
            /* Apply gamma if requested */
            if (gamma != 1.0f)
            {
                if (an <= 0.0f)
                    an = 0.0f;
                else if (an >= 1.0f)
                    an = 1.0f;
                else
                    an = powf(an, gamma);
            }
            if (an >= thr)
            {
                size_t idx = (size_t) (x >> 5);
                dst_row[idx] |= (1u << (x & 31));
                coll++;
            }
            tot++;
        }
    }
    j->collision_pixels = coll;
    j->total_pixels = tot;
}

static void stripe_worker(void* user)
{
    StripeJob* j = (StripeJob*) user;
    stripe_job_run(j);
    if (j->done_sem)
        (void) rogue_sem_post(j->done_sem);
}

/* Distance Field Generation (Signed) ---------------------------------------------------------- */
/* We compute a simple 3x3 chamfer distance transform for inside and outside, then combine to
   produce signed distances (positive inside). Values are stored scaled by scale (default 10). */
static void generate_distance_field(RogueHitPixelMaskFrame* frame, int scale)
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
    if (!frame || !frame->bits || frame->distance_field)
        return;
    int w = frame->width, h = frame->height;
    size_t count = (size_t) w * (size_t) h;
    int16_t* buf = (int16_t*) malloc(count * sizeof(int16_t));
    if (!buf)
        return;
    int16_t* inside = (int16_t*) malloc(count * sizeof(int16_t));
    int16_t* outside = (int16_t*) malloc(count * sizeof(int16_t));
    if (!inside || !outside)
    {
        free(buf);
        free(inside);
        free(outside);
        return;
    }
    const int INF = 32767;
    /* Initialize distance arrays */
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int solid = rogue_hit_mask_test(frame, x, y);
            inside[y * w + x] = (int16_t) (solid ? 0 : INF);
            outside[y * w + x] = (int16_t) (solid ? INF : 0);
        }
    }
    /* Chamfer passes (weights 3/4 approximating Euclidean) */
    const int w1 = 3 * scale; /* orthogonal */
    const int w2 = 4 * scale; /* diagonal */
    /* Forward */
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int idx = y * w + x;
            int16_t d_in = inside[idx];
            int16_t d_out = outside[idx];
            if (x > 0)
            {
                int idx_l = idx - 1;
                if (inside[idx_l] + w1 < d_in)
                    d_in = inside[idx_l] + w1;
                if (outside[idx_l] + w1 < d_out)
                    d_out = outside[idx_l] + w1;
            }
            if (y > 0)
            {
                int idx_u = idx - w;
                if (inside[idx_u] + w1 < d_in)
                    d_in = inside[idx_u] + w1;
                if (outside[idx_u] + w1 < d_out)
                    d_out = outside[idx_u] + w1;
                if (x > 0)
                {
                    int idx_ul = idx_u - 1;
                    if (inside[idx_ul] + w2 < d_in)
                        d_in = inside[idx_ul] + w2;
                    if (outside[idx_ul] + w2 < d_out)
                        d_out = outside[idx_ul] + w2;
                }
                if (x + 1 < w)
                {
                    int idx_ur = idx_u + 1;
                    if (inside[idx_ur] + w2 < d_in)
                        d_in = inside[idx_ur] + w2;
                    if (outside[idx_ur] + w2 < d_out)
                        d_out = outside[idx_ur] + w2;
                }
            }
            inside[idx] = (int16_t) (d_in > 32767 ? 32767 : d_in);
            outside[idx] = (int16_t) (d_out > 32767 ? 32767 : d_out);
        }
    }
    /* Backward */
    for (int y = h - 1; y >= 0; --y)
    {
        for (int x = w - 1; x >= 0; --x)
        {
            int idx = y * w + x;
            int16_t d_in = inside[idx];
            int16_t d_out = outside[idx];
            if (x + 1 < w)
            {
                int idx_r = idx + 1;
                if (inside[idx_r] + w1 < d_in)
                    d_in = inside[idx_r] + w1;
                if (outside[idx_r] + w1 < d_out)
                    d_out = outside[idx_r] + w1;
            }
            if (y + 1 < h)
            {
                int idx_d = idx + w;
                if (inside[idx_d] + w1 < d_in)
                    d_in = inside[idx_d] + w1;
                if (outside[idx_d] + w1 < d_out)
                    d_out = outside[idx_d] + w1;
                if (x > 0)
                {
                    int idx_dl = idx_d - 1;
                    if (inside[idx_dl] + w2 < d_in)
                        d_in = inside[idx_dl] + w2;
                    if (outside[idx_dl] + w2 < d_out)
                        d_out = outside[idx_dl] + w2;
                }
                if (x + 1 < w)
                {
                    int idx_dr = idx_d + 1;
                    if (inside[idx_dr] + w2 < d_in)
                        d_in = inside[idx_dr] + w2;
                    if (outside[idx_dr] + w2 < d_out)
                        d_out = outside[idx_dr] + w2;
                }
            }
            inside[idx] = (int16_t) (d_in > 32767 ? 32767 : d_in);
            outside[idx] = (int16_t) (d_out > 32767 ? 32767 : d_out);
        }
    }
    /* Combine into signed distance: inside-dist - outside-dist. Clamp to int16 range. */
    for (size_t i = 0; i < count; ++i)
    {
        int v = (int) outside[i] - (int) inside[i]; /* positive inside (0 boundary) */
        if (v > 32767)
            v = 32767;
        if (v < -32768)
            v = -32768;
        buf[i] = (int16_t) v;
    }
    free(inside);
    free(outside);
    frame->distance_field = buf;
    frame->distance_field_scale = scale;
#if defined(_MSC_VER)
#pragma warning(pop)
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
/* Helper: apply a simple separable 1-2-1 smoothing pass on a binary occupancy buffer.
   The input is interpreted as 0/1 per pixel. Writes into a temporary float grid and
   thresholds back to binary (>=0.5) to produce a lightly smoothed mask.
   This is conservative: smoothing only adds bits when neighborhood support exists.
*/
static inline int bit_get(const uint32_t* bits, int w, int h, int pitch_words, int x, int y)
{
    if ((unsigned) x >= (unsigned) w || (unsigned) y >= (unsigned) h)
        return 0;
    const uint32_t* row = bits + (size_t) y * (size_t) pitch_words;
    return (int) ((row[(size_t) (x >> 5)] >> (x & 31)) & 1u);
}

static void smooth_occupancy_binary(const uint32_t* in_bits, int w, int h, int pitch_words,
                                    uint32_t* out_bits)
{
    /* Horizontal pass into float row buffer, then vertical with threshold. */
    float* tmp = (float*) calloc((size_t) w * (size_t) h, sizeof(float));
    if (!tmp)
    {
        /* Fallback: copy input. */
        memcpy(out_bits, in_bits, (size_t) pitch_words * (size_t) h * sizeof(uint32_t));
        return;
    }
    /* Horizontal 1-2-1 */
    for (int y = 0; y < h; ++y)
    {
        float* rowf = tmp + (size_t) y * w;
        for (int x = 0; x < w; ++x)
        {
            int a = bit_get(in_bits, w, h, pitch_words, x - 1, y);
            int b = bit_get(in_bits, w, h, pitch_words, x, y);
            int c = bit_get(in_bits, w, h, pitch_words, x + 1, y);
            rowf[x] = (a + 2.0f * b + c) * (1.0f / 4.0f);
        }
    }
    /* Vertical 1-2-1 + threshold back to bits */
    memset(out_bits, 0, (size_t) pitch_words * (size_t) h * sizeof(uint32_t));
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            float a = (y > 0) ? tmp[(size_t) (y - 1) * w + x] : 0.0f;
            float b = tmp[(size_t) y * w + x];
            float c = (y + 1 < h) ? tmp[(size_t) (y + 1) * w + x] : 0.0f;
            float v = (a + 2.0f * b + c) * (1.0f / 4.0f);
            if (v >= 0.5f)
            {
                size_t idx = (size_t) y * pitch_words + (size_t) (x >> 5);
                out_bits[idx] |= (1u << (x & 31));
            }
        }
    }
    free(tmp);
}

static int generate_mipmaps(struct RogueHitPixelMaskFrame* frame, int requested_levels,
                            RoguePixelMaskMetrics* metrics, int smoothing_passes)
{
    if (!frame || requested_levels <= 1)
    {
        if (frame)
        {
            frame->mipmap_count = 1;
            frame->mipmaps = NULL;
        }
        if (metrics)
        {
            metrics->mipmap_levels = 1;
            /* Provide base-level stats when caller wants per-mip arrays */
            if (!metrics->mip_total_pixels && !metrics->mip_collision_pixels &&
                !metrics->mip_solid_ratio)
            {
                /* leave NULLs; caller didn’t request detailed metrics */
            }
            else if (metrics->mip_total_pixels && metrics->mip_collision_pixels &&
                     metrics->mip_solid_ratio)
            {
                metrics->mip_total_pixels[0] =
                    (uint32_t) ((uint64_t) frame->width * (uint64_t) frame->height);
                /* collision_pixels for base already computed in build; compute ratio directly */
                metrics->mip_collision_pixels[0] = metrics->collision_pixels;
                metrics->mip_solid_ratio[0] =
                    metrics->total_pixels
                        ? (float) metrics->collision_pixels / (float) metrics->total_pixels
                        : 0.0f;
            }
            metrics->mip_conservative_monotonic = 1; /* vacuously true */
        }
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
        {
            metrics->mipmap_levels = 1;
            if (metrics->mip_total_pixels && metrics->mip_collision_pixels &&
                metrics->mip_solid_ratio)
            {
                metrics->mip_total_pixels[0] =
                    (uint32_t) ((uint64_t) frame->width * (uint64_t) frame->height);
                metrics->mip_collision_pixels[0] = metrics->collision_pixels;
                metrics->mip_solid_ratio[0] =
                    metrics->total_pixels
                        ? (float) metrics->collision_pixels / (float) metrics->total_pixels
                        : 0.0f;
            }
            metrics->mip_conservative_monotonic = 1;
        }
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
    /* If metrics provided per-mip arrays, fill base immediately and compute others after build */
    if (metrics && metrics->mip_total_pixels && metrics->mip_collision_pixels &&
        metrics->mip_solid_ratio)
    {
        metrics->mip_total_pixels[0] =
            (uint32_t) ((uint64_t) frame->width * (uint64_t) frame->height);
        metrics->mip_collision_pixels[0] = metrics->collision_pixels;
        metrics->mip_solid_ratio[0] = metrics->total_pixels ? (float) metrics->collision_pixels /
                                                                  (float) metrics->total_pixels
                                                            : 0.0f;
    }

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
        /* Optional smoothing on parent occupancy prior to downsample. */
        uint32_t* smoothed_parent = NULL;
        if (smoothing_passes > 0 && prev_bits)
        {
            smoothed_parent =
                (uint32_t*) calloc((size_t) prev_pitch_words * (size_t) prev_h, sizeof(uint32_t));
            if (smoothed_parent)
            {
                smooth_occupancy_binary(prev_bits, prev_w, prev_h, prev_pitch_words,
                                        smoothed_parent);
                prev_bits = smoothed_parent;
            }
        }
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
        if (smoothed_parent)
            free(smoothed_parent);
        prev_bits = ml->bits;
        prev_w = ml->width;
        prev_h = ml->height;
        prev_pitch_words = ml->pitch_words;

        /* Metrics per level: count set bits conservatively and fill arrays if present */
        if (metrics)
        {
            if (metrics->mip_total_pixels && metrics->mip_collision_pixels &&
                metrics->mip_solid_ratio)
            {
                uint32_t total = (uint32_t) ((uint64_t) ml->width * (uint64_t) ml->height);
                uint32_t coll = 0;
                for (int y = 0; y < ml->height; ++y)
                {
                    const uint32_t* row = ml->bits + (size_t) y * ml->pitch_words;
                    for (int xw = 0; xw < ml->pitch_words; ++xw)
                    {
                        uint32_t v = row[xw];
                        v = v - ((v >> 1) & 0x55555555u);
                        v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
                        v = (v + (v >> 4)) & 0x0F0F0F0Fu;
                        v = v + (v >> 8);
                        v = v + (v >> 16);
                        coll += v & 0x3Fu;
                    }
                }
                metrics->mip_total_pixels[level] = total;
                metrics->mip_collision_pixels[level] = coll;
                metrics->mip_solid_ratio[level] = total ? (float) coll / (float) total : 0.0f;
            }
        }
    }
    if (metrics)
    {
        metrics->mipmap_levels = max_levels;
        /* Monotonicity check: occupancy should be non-increasing across levels */
        metrics->mip_conservative_monotonic = 1;
        if (metrics->mip_collision_pixels)
        {
            for (int i = 1; i < max_levels; ++i)
            {
                if (metrics->mip_collision_pixels[i] > metrics->mip_collision_pixels[i - 1])
                {
                    metrics->mip_conservative_monotonic = 0;
                    break;
                }
            }
        }
    }
    return 1;
}

int rogue_pixel_mask_build_from_surface(void* sdl_surface_v, const RoguePixelMaskLoadConfig* cfg,
                                        struct RogueHitPixelMaskFrame* out_frame,
                                        RoguePixelMaskMetrics* out_metrics)
{
    if (out_metrics)
    {
        /* Preserve any caller-provided per-mip arrays; avoid memset to keep pointers intact. */
        uint32_t* preserve_mip_total = out_metrics->mip_total_pixels;
        uint32_t* preserve_mip_coll = out_metrics->mip_collision_pixels;
        float* preserve_mip_ratio = out_metrics->mip_solid_ratio;
        /* Zero scalar fields explicitly */
        out_metrics->total_pixels = 0;
        out_metrics->collision_pixels = 0;
        out_metrics->solid_ratio = 0.0f;
        out_metrics->build_time_ns = 0;
        out_metrics->memory_footprint = 0;
        out_metrics->compressed_size = 0;
        out_metrics->compression_ratio = 1.0f;
        out_metrics->mipmap_levels = 0;
        out_metrics->mip_conservative_monotonic = 0;
        /* Restore pointers */
        out_metrics->mip_total_pixels = preserve_mip_total;
        out_metrics->mip_collision_pixels = preserve_mip_coll;
        out_metrics->mip_solid_ratio = preserve_mip_ratio;
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
    const int bpp = surf->format->BytesPerPixel;
    const unsigned char* base = (const unsigned char*) surf->pixels;

    int used_threads = 0;
    /* Use stripe-based threading if a pool is registered and the image is tall enough. */
    if (g_registered_thread_pool && g_registered_thread_pool->threads &&
        g_registered_thread_pool->thread_count > 1 && h >= 32)
    {
        const int tc = g_registered_thread_pool->thread_count;
        int stripes = tc;
        if (stripes > h)
            stripes = h;
        RogueSem done_sem;
        if (rogue_sem_init(&done_sem, 0) == 0)
        {
            StripeJob* jobs = (StripeJob*) calloc((size_t) stripes, sizeof(StripeJob));
            if (jobs)
            {
                int rows_per = h / stripes;
                int rem = h % stripes;
                int y = 0;
                for (int i = 0; i < stripes; ++i)
                {
                    int take = rows_per + (i < rem ? 1 : 0);
                    jobs[i].base = base;
                    jobs[i].pitch = surf->pitch;
                    jobs[i].bpp = bpp;
                    jobs[i].fmt = surf->format;
                    jobs[i].w = w;
                    jobs[i].h = h;
                    jobs[i].y0 = y;
                    jobs[i].y1 = y + take;
                    jobs[i].thresh = thresh;
                    jobs[i].alpha_gamma = (cfg->alpha_gamma > 0.f) ? cfg->alpha_gamma : 1.0f;
                    jobs[i].derive_alpha_from_luma = cfg->derive_alpha_from_luma ? 1 : 0;
                    jobs[i].out_bits = out_frame->bits;
                    jobs[i].out_pitch_words = out_frame->pitch_words;
                    jobs[i].collision_pixels = 0;
                    jobs[i].total_pixels = 0;
                    jobs[i].done_sem = &done_sem;
                    y += take;
                }
                /* Submit all stripes; if queue is full, run inline. */
                for (int i = 0; i < stripes; ++i)
                {
                    int ok =
                        rogue_thread_pool_submit(g_registered_thread_pool, stripe_worker, &jobs[i]);
                    if (ok != 0)
                    {
                        /* Fallback: run inline to avoid blocking */
                        stripe_worker(&jobs[i]);
                    }
                }
                /* Wait for all stripes to complete */
                for (int i = 0; i < stripes; ++i)
                {
                    (void) rogue_sem_wait(&done_sem);
                }
                /* Reduce metrics deterministically */
                if (out_metrics)
                {
                    for (int i = 0; i < stripes; ++i)
                    {
                        out_metrics->collision_pixels += jobs[i].collision_pixels;
                        out_metrics->total_pixels += jobs[i].total_pixels;
                    }
                }
                free(jobs);
                used_threads = stripes;
            }
            rogue_sem_destroy(&done_sem);
        }
    }
    if (!used_threads)
    {
        /* Fallback: single-threaded build */
        for (int y = 0; y < h; y++)
        {
            const unsigned char* row = base + (size_t) y * (size_t) surf->pitch;
            for (int x = 0; x < w; x++)
            {
                const unsigned char* px = row + (size_t) x * (size_t) bpp;
                float an = 1.0f;
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
                    an = (float) aa * (1.0f / 255.0f);
                }
                else if (cfg->derive_alpha_from_luma)
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
                    float Y = 0.2126f * (float) r + 0.7152f * (float) g + 0.0722f * (float) b;
                    an = Y * (1.0f / 255.0f);
                }
                /* Apply gamma */
                if (cfg->alpha_gamma > 0.0f && cfg->alpha_gamma != 1.0f)
                {
                    if (an <= 0.0f)
                        an = 0.0f;
                    else if (an >= 1.0f)
                        an = 1.0f;
                    else
                        an = powf(an, cfg->alpha_gamma);
                }
                if (an >= thresh)
                {
                    rogue_hit_mask_set(out_frame, x, y);
                    if (out_metrics)
                        out_metrics->collision_pixels++;
                }
                if (out_metrics)
                    out_metrics->total_pixels++;
            }
        }
    }
    /* Optional base-level despeckle smoothing: remove isolated single pixels and fill single-pixel
     * holes */
    if (cfg->edge_smoothing_passes > 0 && out_frame->bits)
    {
        const int passes = (cfg->edge_smoothing_passes > 2) ? 2 : cfg->edge_smoothing_passes;
        const int W = out_frame->width;
        const int H = out_frame->height;
        const int PW = out_frame->pitch_words;
        size_t words_n = (size_t) PW * (size_t) H;
        uint32_t* tmp = (uint32_t*) calloc(words_n, sizeof(uint32_t));
        if (tmp)
        {
            for (int p = 0; p < passes; ++p)
            {
                /* Pass A: remove isolated single pixels */
                memset(tmp, 0, words_n * sizeof(uint32_t));
                for (int y = 0; y < H; ++y)
                {
                    for (int x = 0; x < W; ++x)
                    {
                        int self = bit_get(out_frame->bits, W, H, PW, x, y);
                        int n = 0;
                        n += bit_get(out_frame->bits, W, H, PW, x - 1, y);
                        n += bit_get(out_frame->bits, W, H, PW, x + 1, y);
                        n += bit_get(out_frame->bits, W, H, PW, x, y - 1);
                        n += bit_get(out_frame->bits, W, H, PW, x, y + 1);
                        if (self)
                        {
                            if (n >= 1)
                            {
                                size_t idx = (size_t) y * PW + (size_t) (x >> 5);
                                tmp[idx] |= (1u << (x & 31));
                            }
                        }
                        else
                        {
                            /* Pass B: fill single-pixel holes */
                            if (n >= 3)
                            {
                                size_t idx = (size_t) y * PW + (size_t) (x >> 5);
                                tmp[idx] |= (1u << (x & 31));
                            }
                        }
                    }
                }
                /* swap */
                memcpy(out_frame->bits, tmp, words_n * sizeof(uint32_t));
            }
            free(tmp);
            /* Recompute metrics (collision/total) deterministically after smoothing) */
            if (out_metrics)
            {
                uint32_t coll = 0;
                for (int y = 0; y < H; ++y)
                {
                    const uint32_t* row = out_frame->bits + (size_t) y * PW;
                    for (int xw = 0; xw < PW; ++xw)
                    {
                        uint32_t v = row[xw];
                        /* portable bit count */
                        v = v - ((v >> 1) & 0x55555555u);
                        v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
                        v = (v + (v >> 4)) & 0x0F0F0F0Fu;
                        v = v + (v >> 8);
                        v = v + (v >> 16);
                        coll += v & 0x3Fu; /* 6 bits are enough since max popcount 32 */
                    }
                }
                out_metrics->collision_pixels = coll;
                out_metrics->total_pixels = (uint32_t) ((uint64_t) W * (uint64_t) H);
            }
        }
    }
    SDL_UnlockSurface(surf);
    /* Compression
       compression_level semantics:
         0  = force no compression
         >0 = force RLE (format=1)
         <0 = AUTO: choose between none and RLE based on size benefit
    */
    if (cfg->compression_level == 0)
    {
        /* none */
    }
    else if (cfg->compression_level < 0)
    {
        size_t rle_size = 0;
        void* rle_buf = rle_compress_words(out_frame->bits, words, &rle_size);
        size_t raw_size = words * sizeof(uint32_t);
        if (rle_buf && rle_size < raw_size)
        {
            out_frame->compressed = rle_buf;
            out_frame->compressed_size = rle_size;
            out_frame->compressed_format = 1;
        }
        else
        {
            if (rle_buf)
                free(rle_buf);
            out_frame->compressed = NULL;
            out_frame->compressed_size = 0;
            out_frame->compressed_format = 0;
        }
        if (out_metrics)
            out_metrics->compressed_size = out_frame->compressed ? out_frame->compressed_size : 0;
    }
    else /* >0: force RLE */
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
        /* condition is runtime-configurable even if optimizer sees constant during tests */
        if (!generate_mipmaps(out_frame, cfg->mipmap_levels, out_metrics,
                              cfg->edge_smoothing_passes))
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
        out_metrics->compression_ratio =
            out_metrics->memory_footprint
                ? (out_metrics->compressed_size ? (float) out_metrics->compressed_size /
                                                      (float) out_metrics->memory_footprint
                                                : 1.0f)
                : 1.0f;
        /* If caller provided per-mip arrays and only requested 1 level, populate base here */
        if (cfg->mipmap_levels <= 1 && out_metrics->mip_total_pixels &&
            out_metrics->mip_collision_pixels && out_metrics->mip_solid_ratio)
        {
            out_metrics->mip_total_pixels[0] =
                (uint32_t) ((uint64_t) out_frame->width * (uint64_t) out_frame->height);
            out_metrics->mip_collision_pixels[0] = out_metrics->collision_pixels;
            out_metrics->mip_solid_ratio[0] = out_metrics->solid_ratio;
            out_metrics->mip_conservative_monotonic = 1;
        }
    }
    if (out_metrics && (out_metrics->mip_total_pixels || out_metrics->mip_collision_pixels ||
                        out_metrics->mip_solid_ratio))
    {
        /* per-mip arrays present; metrics populated */
    }
    /* Distance field (optional) */
    if (cfg->generate_distance_fields)
    {
        generate_distance_field(out_frame, 10);
    }
    return 1;
#endif
}

int rogue_pixel_mask_load_from_file(const char* path, const RoguePixelMaskLoadConfig* cfg,
                                    struct RogueHitPixelMaskFrame* out_frame,
                                    RoguePixelMaskMetrics* out_metrics)
{
    if (out_metrics)
    {
        uint32_t* preserve_mip_total = out_metrics->mip_total_pixels;
        uint32_t* preserve_mip_coll = out_metrics->mip_collision_pixels;
        float* preserve_mip_ratio = out_metrics->mip_solid_ratio;
        out_metrics->total_pixels = 0;
        out_metrics->collision_pixels = 0;
        out_metrics->solid_ratio = 0.0f;
        out_metrics->build_time_ns = 0;
        out_metrics->memory_footprint = 0;
        out_metrics->compressed_size = 0;
        out_metrics->compression_ratio = 1.0f;
        out_metrics->mipmap_levels = 0;
        out_metrics->mip_conservative_monotonic = 0;
        out_metrics->mip_total_pixels = preserve_mip_total;
        out_metrics->mip_collision_pixels = preserve_mip_coll;
        out_metrics->mip_solid_ratio = preserve_mip_ratio;
    }
    if (!path || !out_frame)
        return 0;
#if !defined(ROGUE_HAVE_SDL)
    (void) path;
    (void) cfg;
    return 0;
#else
    SDL_Surface* surf = NULL;
    /* Prefer SDL_image when available; otherwise fall back to magic-number detection. */
#if defined(ROGUE_HAVE_SDL_IMAGE)
    surf = IMG_Load(path);
#endif
    if (!surf)
    {
        /* Try to detect by magic numbers instead of extension to avoid mislabeling. */
        unsigned char hdr[64];
        size_t r = 0;
        SDL_RWops* rw = SDL_RWFromFile(path, "rb");
        if (rw)
        {
            r = (size_t) SDL_RWread(rw, hdr, 1, sizeof(hdr));
            SDL_RWclose(rw);
        }
        int is_png = 0, is_bmp = 0, is_dds = 0, is_tga = 0;
        if (r >= 8)
        {
            /* PNG: 89 50 4E 47 0D 0A 1A 0A */
            const unsigned char png_sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
            is_png = (memcmp(hdr, png_sig, 8) == 0);
        }
        if (r >= 2)
        {
            /* BMP: 'B' 'M' */
            is_bmp = (hdr[0] == 'B' && hdr[1] == 'M');
        }
        if (r >= 4)
        {
            /* DDS: 'D' 'D' 'S' ' ' */
            is_dds = (hdr[0] == 'D' && hdr[1] == 'D' && hdr[2] == 'S' && hdr[3] == ' ');
        }
        if (!is_png && !is_bmp && !is_dds)
        {
            /* Weak TGA detection: many TGAs lack a strong header. We avoid false positives
               and rely on SDL_image when present. Mark as TGA only if footer exists: */
            SDL_RWops* rwt = SDL_RWFromFile(path, "rb");
            if (rwt)
            {
                if (SDL_RWseek(rwt, -26, RW_SEEK_END) >= 0) /* 26-byte signature footer */
                {
                    unsigned char footer[26];
                    size_t rr = (size_t) SDL_RWread(rwt, footer, 1, sizeof(footer));
                    if (rr == sizeof(footer) && memcmp(footer + 8, "TRUEVISION-XFILE.", 18) == 0)
                        is_tga = 1;
                }
                SDL_RWclose(rwt);
            }
        }
        if (is_bmp)
        {
            surf = SDL_LoadBMP(path);
        }
        else if (is_png)
        {
#if defined(ROGUE_HAVE_SDL_IMAGE)
            /* If we got here with SDL_image present, IMG_Load already failed; still try once. */
            surf = IMG_Load(path);
#endif
        }
        else if (is_dds || is_tga)
        {
            ROGUE_LOG_DEBUG("pixel_mask_loader: %s requires SDL_image (not available): %s",
                            is_dds ? "DDS" : "TGA", path);
        }
        if (!surf)
        {
            ROGUE_LOG_DEBUG("pixel_mask_load_fallback_placeholder: %s", path);
            return 0; /* caller may fall back to placeholder */
        }
    }
    int ok = rogue_pixel_mask_build_from_surface(surf, cfg, out_frame, out_metrics);
    SDL_FreeSurface(surf);
    return ok;
#endif
}

typedef struct AsyncPixelMaskJob
{
    SDL_Surface* surf;
    RoguePixelMaskLoadConfig cfg;
    RogueHitPixelMaskFrame* out_frame;
    RoguePixelMaskMetrics* out_metrics;
} AsyncPixelMaskJob;

static void async_build_job(void* user)
{
    AsyncPixelMaskJob* job = (AsyncPixelMaskJob*) user;
    rogue_pixel_mask_build_from_surface(job->surf, &job->cfg, job->out_frame, job->out_metrics);
    SDL_FreeSurface(job->surf);
    free(job);
}

int rogue_pixel_mask_build_async(void* sdl_surface, const RoguePixelMaskLoadConfig* cfg,
                                 struct RogueHitPixelMaskFrame* out_frame,
                                 RoguePixelMaskMetrics* out_metrics, int use_thread_pool)
{
#if !defined(ROGUE_HAVE_SDL)
    (void) sdl_surface;
    (void) cfg;
    (void) out_frame;
    (void) out_metrics;
    (void) use_thread_pool;
    return 0;
#else
    if (!sdl_surface)
        return 0;
    if (!use_thread_pool)
        return rogue_pixel_mask_build_from_surface(sdl_surface, cfg, out_frame, out_metrics);
    if (!g_registered_thread_pool || !g_registered_thread_pool->threads ||
        g_registered_thread_pool->thread_count <= 0)
        return rogue_pixel_mask_build_from_surface(sdl_surface, cfg, out_frame, out_metrics);
    AsyncPixelMaskJob* job = (AsyncPixelMaskJob*) malloc(sizeof(AsyncPixelMaskJob));
    if (!job)
        return 0;
    job->surf = (SDL_Surface*) sdl_surface;
    job->cfg = cfg ? *cfg : rogue_pixel_mask_load_config_default();
    job->out_frame = out_frame;
    job->out_metrics = out_metrics;
    int ok = rogue_thread_pool_submit(g_registered_thread_pool, async_build_job, job);
    if (ok != 0)
        return 1; /* enqueued */
    /* fallback */
    free(job);
    return rogue_pixel_mask_build_from_surface(sdl_surface, cfg, out_frame, out_metrics);
#endif
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
