/* sprite_atlas_collision.c - Milestone 1.3 (initial slice implementation) */
#include "game/sprite_atlas_collision.h"
#include "game/hit_pixel_mask.h"
#include <stdlib.h>
#include <string.h>

static int alloc_frame_basic(struct RogueHitPixelMaskFrame* f, int w, int h)
{
    if (!f || w <= 0 || h <= 0)
        return 0;
    memset(f, 0, sizeof(*f));
    f->width = w;
    f->height = h;
    f->pitch_words = (w + 31) / 32;
    f->mipmap_count = 1; /* base only */
    size_t words = (size_t) f->pitch_words * (size_t) h;
    f->bits = (uint32_t*) calloc(words, sizeof(uint32_t));
    return f->bits != NULL;
}

int rogue_sprite_atlas_extract_collision_region(const struct RogueHitPixelMaskFrame* atlas,
                                                const RogueSpriteAtlasRegion* region,
                                                struct RogueHitPixelMaskFrame* out_frame)
{
    if (!atlas || !region || !out_frame)
        return 0;
    if (region->w == 0 || region->h == 0)
        return 0;
    if (region->x + region->w > (uint16_t) atlas->width ||
        region->y + region->h > (uint16_t) atlas->height)
        return 0; /* OOB */
    if (!alloc_frame_basic(out_frame, region->w, region->h))
        return 0;
    out_frame->origin_x = region->origin_x;
    out_frame->origin_y = region->origin_y;
    for (uint16_t ry = 0; ry < region->h; ++ry)
    {
        for (uint16_t rx = 0; rx < region->w; ++rx)
        {
            int ax = region->x + rx;
            int ay = region->y + ry;
            if (rogue_hit_mask_test(atlas, ax, ay))
                rogue_hit_mask_set(out_frame, rx, ry);
        }
    }
    return 1;
}

void rogue_sprite_atlas_region_free_frame(struct RogueHitPixelMaskFrame* f)
{
    if (!f)
        return;
    free(f->bits);
    f->bits = NULL;
    f->width = f->height = 0;
    f->pitch_words = 0;
}

int rogue_animation_collision_build(RogueAnimationCollisionSet* set,
                                    const struct RogueHitPixelMaskFrame** frames,
                                    const uint32_t* frame_durations_ms, uint8_t frame_count,
                                    float interpolation_factor)
{
    if (!set || !frames || !frame_durations_ms || frame_count == 0)
        return 0;
    memset(set, 0, sizeof(*set));
    set->frame_count = frame_count;
    set->interpolation_factor = interpolation_factor;
    set->using_inline = frame_count <= ROGUE_ANIM_INLINE_MAX_FRAMES;
    if (set->using_inline)
    {
        set->frames = set->frames_inline;
        set->frame_timings = set->frame_timings_inline;
    }
    else
    {
        set->frames = (const struct RogueHitPixelMaskFrame**) malloc(sizeof(*frames) * frame_count);
        set->frame_timings = (uint32_t*) malloc(sizeof(uint32_t) * (frame_count + 1));
        if (!set->frames || !set->frame_timings)
        {
            free(set->frames);
            free(set->frame_timings);
            memset(set, 0, sizeof(*set));
            return 0;
        }
    }
    uint32_t cumulative = 0;
    set->frame_timings[0] = 0;
    for (uint8_t i = 0; i < frame_count; ++i)
    {
        set->frames[i] = frames[i];
        uint32_t d = frame_durations_ms[i];
        if (d == 0)
            d = 1;
        cumulative += d;
        set->frame_timings[i + 1] = cumulative;
    }
    set->total_duration_ms = cumulative;
    return 1;
}

void rogue_animation_collision_free(RogueAnimationCollisionSet* set)
{
    if (!set)
        return;
    if (!set->using_inline)
    {
        free(set->frames);
        free(set->frame_timings);
    }
    if (set->blended_frame)
    {
        free(set->blended_frame->bits);
        free(set->blended_frame);
    }
    memset(set, 0, sizeof(*set));
}

int rogue_animation_collision_sample(const RogueAnimationCollisionSet* set, uint32_t t_ms,
                                     const struct RogueHitPixelMaskFrame** out_a,
                                     const struct RogueHitPixelMaskFrame** out_b, float* out_t)
{
    if (!set || set->frame_count == 0 || set->total_duration_ms == 0)
        return 0;
    uint32_t mod = t_ms % set->total_duration_ms;
    uint8_t idx = 0;
    while (idx < set->frame_count && set->frame_timings[idx + 1] <= mod)
        ++idx;
    if (idx >= set->frame_count)
        idx = set->frame_count - 1;
    uint32_t start = set->frame_timings[idx];
    uint32_t end = set->frame_timings[idx + 1];
    float span = (float) (end - start);
    float local_t = span > 0.f ? (float) (mod - start) / span : 0.f;
    if (local_t < 0.f)
        local_t = 0.f;
    if (local_t > 1.f)
        local_t = 1.f;
    uint8_t next = (idx + 1) % set->frame_count;
    if (out_a)
        *out_a = set->frames[idx];
    if (out_b)
        *out_b = set->frames[next];
    if (out_t)
        *out_t = local_t * set->interpolation_factor;
    return 1;
}

/* Ensure scratch blended frame of given dimensions (realloc if size changed). */
static struct RogueHitPixelMaskFrame* ensure_blend_capacity(RogueAnimationCollisionSet* set, int w,
                                                            int h)
{
    if (!set)
        return NULL;
    if (set->blended_frame && (set->blended_w != w || set->blended_h != h))
    {
        free(set->blended_frame->bits);
        free(set->blended_frame);
        set->blended_frame = NULL;
    }
    if (!set->blended_frame)
    {
        set->blended_frame =
            (struct RogueHitPixelMaskFrame*) calloc(1, sizeof(*set->blended_frame));
        if (!set->blended_frame)
            return NULL;
        if (!alloc_frame_basic(set->blended_frame, w, h))
        {
            free(set->blended_frame);
            set->blended_frame = NULL;
            return NULL;
        }
        set->blended_w = w;
        set->blended_h = h;
    }
    return set->blended_frame;
}

const struct RogueHitPixelMaskFrame*
rogue_animation_collision_sample_blended(RogueAnimationCollisionSet* set, uint32_t t_ms,
                                         float* out_local_t)
{
    if (out_local_t)
        *out_local_t = 0.f;
    if (!set)
        return NULL;
    const struct RogueHitPixelMaskFrame *a, *b;
    float local_t = 0.f;
    if (!rogue_animation_collision_sample(set, t_ms, &a, &b, &local_t))
        return NULL;
    if (out_local_t)
        *out_local_t = local_t;
    /* Fast path: near endpoints or identical frames => return A or B directly */
    if (a == b || local_t <= 0.15f)
        return a;
    if (local_t >= 0.85f)
        return b;
    /* Require same dimensions to blend; else (debug) assert and fallback to A. */
    if (a->width != b->width || a->height != b->height)
    {
#include <assert.h>
        assert(
            !"rogue_animation_collision_sample_blended dimension mismatch (future resample slice)");
        return a; /* future slice: resample path */
    }
    struct RogueHitPixelMaskFrame* blend = ensure_blend_capacity(set, a->width, a->height);
    if (!blend)
        return a; /* fallback */
    /* Clear bits */
    size_t words = (size_t) blend->pitch_words * (size_t) blend->height;
    memset(blend->bits, 0, words * sizeof(uint32_t));
    /* Union: OR the words from A then OR from B (bit layout row-major) */
    size_t words_a = (size_t) a->pitch_words * (size_t) a->height;
    if (words_a == words)
    {
        for (size_t i = 0; i < words; ++i)
            blend->bits[i] = a->bits[i] | b->bits[i];
    }
    else
    {
        /* Dimensions mismatch safety fallback already handled before, but keep guard. */
        for (int y = 0; y < a->height && y < blend->height; ++y)
        {
            for (int xw = 0; xw < a->pitch_words && xw < blend->pitch_words; ++xw)
            {
                size_t idx = (size_t) y * blend->pitch_words + xw;
                size_t idx_a = (size_t) y * a->pitch_words + xw;
                size_t idx_b = (size_t) y * b->pitch_words + xw;
                blend->bits[idx] = a->bits[idx_a] | b->bits[idx_b];
            }
        }
    }
    blend->origin_x = a->origin_x; /* simple carry-through */
    blend->origin_y = a->origin_y;
    return blend;
}

void rogue_animation_collision_release_blended(RogueAnimationCollisionSet* set)
{
    if (!set || !set->blended_frame)
        return;
    free(set->blended_frame->bits);
    free(set->blended_frame);
    set->blended_frame = NULL;
    set->blended_w = set->blended_h = 0;
}
