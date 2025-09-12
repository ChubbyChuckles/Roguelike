/**
 * @file hit_pixel_mask.c
 * @brief Pixel-based hit detection system for precise weapon collision detection
 *
 * This module implements a pixel-perfect hit detection system for weapons in the roguelike game.
 * It provides efficient bit-packed pixel masks for accurate collision detection between weapon
 * attack frames and enemy positions, with support for rotation, scaling, and pose transformations.
 *
 * The system uses lazy loading to generate or load pixel masks for weapons on demand, falling
 * back to simple placeholder masks when asset loading is not yet implemented.
 */

#include "hit_pixel_mask.h"
#include "pixel_mask_loader.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define ROGUE_HITMASK_SIMD_SSE2 1
#else
#define ROGUE_HITMASK_SIMD_SSE2 0
#endif

/** @brief Global toggle for enabling pixel mask hit detection (default off until validated) */
int g_hit_use_pixel_masks = 0; /* default off until pixel path validated */
/* Runtime switch to allow tests to force scalar paths even when SSE2 is available. */
static int g_hitmask_simd_enabled = 1;
void rogue_hit_mask_simd_set_enabled(int enabled) { g_hitmask_simd_enabled = enabled ? 1 : 0; }

/** @brief Maximum number of pixel mask sets that can be cached simultaneously */
#define MAX_PIXEL_MASK_SETS 16

/** @brief Static array storing all loaded pixel mask sets */
static RogueHitPixelMaskSet g_sets[MAX_PIXEL_MASK_SETS];

/** @brief Current count of loaded pixel mask sets */
static int g_set_count = 0;

/**
 * @brief Finds an existing pixel mask set for the specified weapon ID
 *
 * Searches through the loaded mask sets to find one matching the given weapon ID.
 *
 * @param weapon_id The weapon identifier to search for
 * @return Pointer to the found mask set, or NULL if not found
 */
static RogueHitPixelMaskSet* find_set(int weapon_id)
{
    for (int i = 0; i < g_set_count; i++)
        if (g_sets[i].weapon_id == weapon_id)
            return &g_sets[i];
    return NULL;
}

/**
 * @brief Allocates and initializes a pixel mask frame with the specified dimensions
 *
 * Creates a bit-packed pixel mask frame with proper memory allocation and initialization.
 * The frame uses 32-bit words for efficient storage and fast bit operations.
 *
 * @param f Pointer to the frame structure to initialize
 * @param w Width of the frame in pixels
 * @param h Height of the frame in pixels
 */
static void alloc_frame(RogueHitPixelMaskFrame* f, int w, int h)
{
    if (!f)
        return;
    if (w <= 0 || h <= 0)
        return;
    f->width = w;
    f->height = h;
    f->origin_x = 0;
    f->origin_y = 0;
    f->pitch_words = (w + 31) / 32;
    size_t words = (size_t) f->pitch_words * (size_t) h;
    f->bits = (uint32_t*) malloc(words * sizeof(uint32_t));
    if (f->bits)
        memset(f->bits, 0, words * sizeof(uint32_t));
}

/**
 * @brief Ensures a pixel mask set exists for the specified weapon ID
 *
 * This function implements lazy loading for pixel mask sets. If a mask set for the weapon
 * already exists and is ready, it returns the cached set. Otherwise, it creates a new set
 * and generates placeholder mask data.
 *
 * The placeholder generation creates simple horizontal bar masks that simulate weapon
 * swing progression over 8 frames, with each frame showing slight forward advancement.
 *
 * @param weapon_id The weapon identifier for which to ensure mask availability
 * @return Pointer to the ready mask set, or NULL if allocation failed
 * @note Currently generates placeholder masks until proper asset loading is implemented
 */
RogueHitPixelMaskSet* rogue_hit_pixel_masks_ensure(int weapon_id)
{
    RogueHitPixelMaskSet* s = find_set(weapon_id);
    if (s && s->ready)
        return s;
    if (!s)
    {
        if (g_set_count >= MAX_PIXEL_MASK_SETS)
            return NULL;
        s = &g_sets[g_set_count++];
        memset(s, 0, sizeof *s);
        s->weapon_id = weapon_id;
    }
    s->frame_count = 8;
    /* Attempt Phase 1 asset-backed load (single frame replicated) once (i==0) */
    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.mipmap_levels = 3; /* request small mip chain for future broad-phase */
    RoguePixelMaskMetrics metrics;
    const char* candidate_paths[] = {"assets/placeholder.png"};
    int loaded_any = 0;
    for (size_t path_i = 0;
         path_i < sizeof(candidate_paths) / sizeof(candidate_paths[0]) && !loaded_any; ++path_i)
    {
        RogueHitPixelMaskFrame tmp = {0};
        if (rogue_pixel_mask_load_from_file(candidate_paths[path_i], &cfg, &tmp, &metrics))
        {
            for (int frame_i = 0; frame_i < 8; ++frame_i)
            {
                s->frames[frame_i] = tmp; /* shallow copy */
                if (frame_i > 0)
                {
                    size_t words = (size_t) tmp.pitch_words * (size_t) tmp.height;
                    s->frames[frame_i].bits = (uint32_t*) malloc(words * sizeof(uint32_t));
                    if (s->frames[frame_i].bits)
                        memcpy(s->frames[frame_i].bits, tmp.bits, words * sizeof(uint32_t));
                    /* Duplicate advanced buffers (compressed + mipmaps) only once to save time;
                     * reuse pointers */
                    s->frames[frame_i].compressed = tmp.compressed;
                    s->frames[frame_i].compressed_size = tmp.compressed_size;
                    s->frames[frame_i].compressed_format = tmp.compressed_format;
                    s->frames[frame_i].mipmap_count = tmp.mipmap_count;
                    s->frames[frame_i].mipmaps = tmp.mipmaps; /* share (read-only) */
                }
            }
            loaded_any = 1;
        }
    }
    if (!loaded_any)
    {
        for (int frame_i = 0; frame_i < 8; ++frame_i)
        {
            alloc_frame(&s->frames[frame_i], 48, 16);
            s->frames[frame_i].mipmap_count = 1;
            s->frames[frame_i].mipmaps = NULL;
            s->frames[frame_i].compressed = NULL;
            s->frames[frame_i].compressed_size = 0;
            s->frames[frame_i].compressed_format = 0;
            int advance = frame_i * 4;
            if (advance > 24)
                advance = 24;
            for (int y = 6; y < 10; y++)
                for (int x = advance; x < advance + 24; ++x)
                    rogue_hit_mask_set(&s->frames[frame_i], x, y);
        }
    }
    s->ready = 1;
    return s;
}

/**
 * @brief Releases all allocated pixel mask data and resets the system
 *
 * This function performs cleanup of all loaded pixel mask sets, freeing allocated
 * memory for individual frames and resetting the global state. Used primarily
 * for test teardown and memory management.
 *
 * @note This function should be called when shutting down the game or during
 *       testing to prevent memory leaks
 */
void rogue_hit_pixel_masks_reset_all(void)
{
    for (int i = 0; i < g_set_count; i++)
    {
        for (int f = 0; f < g_sets[i].frame_count; ++f)
        {
            RogueHitPixelMaskFrame* fr = &g_sets[i].frames[f];
            free(fr->bits);
            fr->bits = NULL;
            /* Only free shared advanced buffers once (for frame 0) */
            if (f == 0)
            {
                if (fr->compressed)
                    free(fr->compressed);
                if (fr->mipmaps)
                {
                    for (int ml = 0; ml < fr->mipmap_count - 1; ++ml)
                    {
                        free(fr->mipmaps[ml].bits);
                    }
                    free(fr->mipmaps);
                }
                if (fr->distance_field)
                    free(fr->distance_field);
            }
            fr->compressed = NULL;
            fr->mipmaps = NULL;
            fr->mipmap_count = 0;
            fr->distance_field = NULL;
        }
    }
    g_set_count = 0;
}
/**
 * @brief Computes the axis-aligned bounding box of a pixel mask frame
 *
 * Returns the dimensions of the mask frame in local mask space, aligned to the origin.
 * This is useful for broad-phase collision detection before performing pixel-perfect tests.
 *
 * @param f Pointer to the pixel mask frame to query
 * @param out_w Pointer to store the width (can be NULL)
 * @param out_h Pointer to store the height (can be NULL)
 * @note Returns 0 for both dimensions if the frame pointer is NULL
 */
void rogue_hit_mask_frame_aabb(const RogueHitPixelMaskFrame* f, int* out_w, int* out_h)
{
    if (out_w)
        *out_w = f ? f->width : 0;
    if (out_h)
        *out_h = f ? f->height : 0;
}
/**
 * @brief Tests enemy collision against a pixel mask frame using multi-point sampling
 *
 * Performs pixel-perfect collision detection between an enemy (represented as a circle)
 * and a weapon attack frame. Uses a two-stage sampling approach:
 * 1. Tests the enemy center point
 * 2. If center misses, tests 8 points around the enemy perimeter at 70% radius
 *
 * This provides efficient yet accurate hit detection for circular enemy hitboxes.
 *
 * @param f Pointer to the pixel mask frame to test against
 * @param enemy_cx_local Enemy center X coordinate in local mask space
 * @param enemy_cy_local Enemy center Y coordinate in local mask space
 * @param enemy_radius Enemy collision radius
 * @param out_lx Pointer to store the local X coordinate of hit point (can be NULL)
 * @param out_ly Pointer to store the local Y coordinate of hit point (can be NULL)
 * @return 1 if enemy collides with mask, 0 otherwise
 * @note Returns 0 if frame is invalid or has no allocated bits
 */
int rogue_hit_mask_enemy_test(const RogueHitPixelMaskFrame* f, float enemy_cx_local,
                              float enemy_cy_local, float enemy_radius, int* out_lx, int* out_ly)
{
    if (!f || !f->bits)
        return 0; /* sample center first */
    int cx = (int) enemy_cx_local, cy = (int) enemy_cy_local;
    if (rogue_hit_mask_test(f, cx, cy))
    {
        if (out_lx)
            *out_lx = cx;
        if (out_ly)
            *out_ly = cy;
        return 1;
    } /* ring sample 8 points */
    float r = enemy_radius * 0.7f;
    for (int i = 0; i < 8; i++)
    {
        float ang = (float) (i * 3.14159265358979323846 * 0.25);
        float sx = enemy_cx_local + r * (float) cos(ang);
        float sy = enemy_cy_local + r * (float) sin(ang);
        int ix = (int) sx, iy = (int) sy;
        if (rogue_hit_mask_test(f, ix, iy))
        {
            if (out_lx)
                *out_lx = ix;
            if (out_ly)
                *out_ly = iy;
            return 1;
        }
    }
    return 0;
}
/**
 * @brief Converts local pixel coordinates to world space coordinates
 *
 * Transforms a pixel position from the local mask coordinate system to world space,
 * accounting for player position, pose offsets, scaling, and rotation. This is essential
 * for accurate hit positioning and visual effects placement.
 *
 * The transformation applies the following steps:
 * 1. Convert pixel coordinates to local space relative to mask origin
 * 2. Apply scaling
 * 3. Apply rotation around the origin
 * 4. Add player position and pose offsets
 *
 * @param f Pointer to the pixel mask frame (used for origin offset, can be NULL)
 * @param lx Local X coordinate in mask space
 * @param ly Local Y coordinate in mask space
 * @param player_x Player's world X position
 * @param player_y Player's world Y position
 * @param pose_dx Additional X offset from player pose
 * @param pose_dy Additional Y offset from player pose
 * @param scale Uniform scaling factor to apply
 * @param angle_rad Rotation angle in radians (counter-clockwise)
 * @param out_wx Pointer to store world X coordinate (can be NULL)
 * @param out_wy Pointer to store world Y coordinate (can be NULL)
 * @note If frame is NULL, returns player position without transformation
 */
void rogue_hit_mask_local_pixel_to_world(const RogueHitPixelMaskFrame* f, int lx, int ly,
                                         float player_x, float player_y, float pose_dx,
                                         float pose_dy, float scale, float angle_rad, float* out_wx,
                                         float* out_wy)
{
    if (!f)
    {
        if (out_wx)
            *out_wx = player_x;
        if (out_wy)
            *out_wy = player_y;
        return;
    }
    float x = (float) (lx - f->origin_x + 0.5f) * scale;
    float y = (float) (ly - f->origin_y + 0.5f) * scale;
    float ca = (float) cos(angle_rad), sa = (float) sin(angle_rad);
    float rx = x * ca - y * sa;
    float ry = x * sa + y * ca;
    if (out_wx)
        *out_wx = player_x + pose_dx + rx;
    if (out_wy)
        *out_wy = player_y + pose_dy + ry;
}

/* ---- Fast rectangle queries ---- */

/* Clip rectangle [x,x+w) x [y,y+h) to frame bounds; return 0 if empty after clip. */
static int clip_rect_to_frame(const RogueHitPixelMaskFrame* f, int* x, int* y, int* w, int* h)
{
    if (!f || !f->bits || *w <= 0 || *h <= 0)
        return 0;
    int x0 = *x, y0 = *y, w0 = *w, h0 = *h;
    if (x0 < 0)
    {
        w0 += x0;
        x0 = 0;
    }
    if (y0 < 0)
    {
        h0 += y0;
        y0 = 0;
    }
    if (x0 + w0 > f->width)
        w0 = f->width - x0;
    if (y0 + h0 > f->height)
        h0 = f->height - y0;
    if (w0 <= 0 || h0 <= 0)
        return 0;
    *x = x0;
    *y = y0;
    *w = w0;
    *h = h0;
    return 1;
}

int rogue_hit_mask_any_set_in_rect(const RogueHitPixelMaskFrame* f, int x, int y, int w, int h)
{
    if (!clip_rect_to_frame(f, &x, &y, &w, &h))
        return 0;
    const int x_end = x + w;
    const int y_end = y + h;
    const uint32_t* bits = f->bits;
    const int pitch = f->pitch_words;
    /* Iterate rows */
    for (int yy = y; yy < y_end; ++yy)
    {
        /* Word range in this row */
        int word_start = x >> 5;
        int word_end = (x_end - 1) >> 5;
        int bit_lo = x & 31;
        int bit_hi = (x_end - 1) & 31;
        const uint32_t* row = bits + (size_t) yy * (size_t) pitch;
        if (word_start == word_end)
        {
            uint32_t mask =
                ((bit_hi == 31) ? 0xFFFFFFFFu : ((1u << (bit_hi + 1)) - 1u)) & (~0u << bit_lo);
            if (row[word_start] & mask)
                return 1;
        }
        else
        {
            /* first partial */
            uint32_t first_mask = ~0u << bit_lo;
            if (row[word_start] & first_mask)
                return 1;
            /* middle full words */
            for (int wi = word_start + 1; wi < word_end; ++wi)
            {
                if (row[wi])
                    return 1;
            }
            /* last partial */
            uint32_t last_mask = (bit_hi == 31) ? 0xFFFFFFFFu : ((1u << (bit_hi + 1)) - 1u);
            if (row[word_end] & last_mask)
                return 1;
        }
    }
    return 0;
}

int rogue_hit_mask_intersect_any_same_origin(const RogueHitPixelMaskFrame* a, int ax, int ay,
                                             const RogueHitPixelMaskFrame* b, int bx, int by, int w,
                                             int h)
{
    if (!a || !b || !a->bits || !b->bits || w <= 0 || h <= 0)
        return 0;
    /* Clip rectangles independently. If sizes diverge after clip, reduce to common size. */
    int ax0 = ax, ay0 = ay, aw = w, ah = h;
    int bx0 = bx, by0 = by, bw = w, bh = h;
    if (!clip_rect_to_frame(a, &ax0, &ay0, &aw, &ah))
        return 0;
    if (!clip_rect_to_frame(b, &bx0, &by0, &bw, &bh))
        return 0;
    /* Compute common width/height */
    if (aw > bw)
        aw = bw;
    if (ah > bh)
        ah = bh;
    if (aw <= 0 || ah <= 0)
        return 0;

    const int a_pitch = a->pitch_words;
    const int b_pitch = b->pitch_words;
    const uint32_t* a_bits = a->bits;
    const uint32_t* b_bits = b->bits;

    const int y_end = ah; /* iterate rows count */

    int a_word_start = ax0 >> 5;
    int b_word_start = bx0 >> 5;
    int a_bit_lo = ax0 & 31;
    int b_bit_lo = bx0 & 31;

    for (int row_i = 0; row_i < y_end; ++row_i)
    {
        const uint32_t* a_row = a_bits + (size_t) (ay0 + row_i) * (size_t) a_pitch;
        const uint32_t* b_row = b_bits + (size_t) (by0 + row_i) * (size_t) b_pitch;

        int a_ws = a_word_start;
        int b_ws = b_word_start;
        int ax_cur = ax0;
        int bx_cur = bx0;
        int remaining = aw;

        /* Handle unaligned first fragment to word boundary */
        if (a_bit_lo != 0 || b_bit_lo != 0)
        {
            int take = 32 - (ax_cur & 31);
            int take_b = 32 - (bx_cur & 31);
            if (take > remaining)
                take = remaining;
            if (take_b > remaining)
                take_b = remaining;
            int frag = take < take_b ? take : take_b;
            uint32_t mask_a = (~0u << (ax_cur & 31)) &
                              ((frag >= 32) ? ~0u : ((1u << ((ax_cur & 31) + frag)) - 1u));
            uint32_t mask_b = (~0u << (bx_cur & 31)) &
                              ((frag >= 32) ? ~0u : ((1u << ((bx_cur & 31) + frag)) - 1u));
            if ((a_row[a_ws] & mask_a) & (b_row[b_ws] & mask_b))
                return 1;
            ax_cur += frag;
            bx_cur += frag;
            remaining -= frag;
            if ((ax_cur & 31) == 0)
                ++a_ws;
            if ((bx_cur & 31) == 0)
                ++b_ws;
        }

        /* Process middle full 32-bit words; if both aligned now, we can use fast path */
        while (remaining >= 32)
        {
#if ROGUE_HITMASK_SIMD_SSE2
            if (g_hitmask_simd_enabled)
            {
                /* Process 4 words at a time when possible */
                if (remaining >= 128)
                {
                    __m128i va = _mm_loadu_si128((const __m128i*) &a_row[a_ws]);
                    __m128i vb = _mm_loadu_si128((const __m128i*) &b_row[b_ws]);
                    __m128i vand = _mm_and_si128(va, vb);
                    /* SSE2-compatible zero check: movemask of compare-to-zero */
                    __m128i vzero = _mm_setzero_si128();
                    __m128i vcmp = _mm_cmpeq_epi8(vand, vzero);
                    int mask = _mm_movemask_epi8(vcmp);
                    if (mask != 0xFFFF)
                        return 1; /* some byte had non-zero intersection */
                    a_ws += 4;
                    b_ws += 4;
                    remaining -= 128;
                    continue;
                }
            }
#endif
            /* Scalar 32-bit step */
            if (a_row[a_ws] & b_row[b_ws])
                return 1;
            ++a_ws;
            ++b_ws;
            remaining -= 32;
        }

        /* Tail (remaining < 32) */
        if (remaining > 0)
        {
            uint32_t tail_mask_a = (remaining == 32) ? ~0u : ((1u << remaining) - 1u);
            uint32_t tail_mask_b = tail_mask_a;
            uint32_t av = a_row[a_ws] & (tail_mask_a << (ax_cur & 31));
            uint32_t bv = b_row[b_ws] & (tail_mask_b << (bx_cur & 31));
            if (av & bv)
                return 1;
        }
    }
    return 0;
}
