/* animation_collision_sync.c - Milestone 3.2 Scaffold Implementation
 *
 * Provides baseline timeline window evaluation and keyframe mask interpolation helpers.
 * Advanced features (multi-window overlap resolution strategies, dynamic speed scaling,
 * spline interpolation, adaptive quality selection, frame skipping heuristics, event
 * batching) are intentionally deferred to later deepening slices. This scaffold focuses
 * on deterministic behavior suitable for unit testing and early integration without
 * introducing performance complexity.
 */

#include "game/animation_collision_sync.h"
#include "game/hit_pixel_mask.h" /* for RogueHitPixelMaskFrame struct */
#include <stddef.h>
#include <stdlib.h> /* malloc/calloc/free */
/* -------------------------------------------------------------------------------------------- */
/* Tiny pooled buffer for blended frames (cache-friendly reuse across instances)                */
/* Fixed-size ring of small frames avoids per-call malloc churn when many syncs morph in turn.  */
/* Pool stores 4 entries; each entry lazily sized up to last requested dimensions.              */
typedef struct BlendedPoolEntry
{
    struct RogueHitPixelMaskFrame* frame;
    int w, h;
    int in_use; /* 0 free, 1 checked out */
} BlendedPoolEntry;

static BlendedPoolEntry g_blend_pool[4];

static struct RogueHitPixelMaskFrame* pool_acquire(int w, int h)
{
    /* Find a free slot or steal the oldest free slot (first-fit). */
    for (int i = 0; i < 4; ++i)
    {
        if (!g_blend_pool[i].in_use)
        {
            g_blend_pool[i].in_use = 1;
            /* Allocate or resize */
            if (!g_blend_pool[i].frame || g_blend_pool[i].w != w || g_blend_pool[i].h != h)
            {
                if (g_blend_pool[i].frame)
                {
                    free(g_blend_pool[i].frame->bits);
                    free(g_blend_pool[i].frame);
                    g_blend_pool[i].frame = NULL;
                }
                struct RogueHitPixelMaskFrame* f =
                    (struct RogueHitPixelMaskFrame*) calloc(1, sizeof(*f));
                if (!f)
                {
                    g_blend_pool[i].in_use = 0;
                    return NULL;
                }
                int pitch_words = (w + 31) / 32;
                f->bits = (uint32_t*) calloc((size_t) pitch_words * (size_t) h, sizeof(uint32_t));
                if (!f->bits)
                {
                    free(f);
                    g_blend_pool[i].in_use = 0;
                    return NULL;
                }
                f->width = w;
                f->height = h;
                f->pitch_words = pitch_words;
                f->origin_x = 0;
                f->origin_y = 0;
                g_blend_pool[i].frame = f;
                g_blend_pool[i].w = w;
                g_blend_pool[i].h = h;
            }
            else
            {
                /* Clear prior contents for deterministic tests */
                size_t words = (size_t) g_blend_pool[i].frame->pitch_words * (size_t) h;
                for (size_t k = 0; k < words; ++k)
                    g_blend_pool[i].frame->bits[k] = 0;
            }
            return g_blend_pool[i].frame;
        }
    }
    return NULL; /* pool exhausted */
}

static void pool_release(struct RogueHitPixelMaskFrame* f)
{
    if (!f)
        return;
    for (int i = 0; i < 4; ++i)
    {
        if (g_blend_pool[i].frame == f)
        {
            g_blend_pool[i].in_use = 0;
            return;
        }
    }
}

/* Forward declare skill layer frame index helper (defined in skill_collision_manager.c) */
float rogue_skill_collision_layer_frame_index(const struct RogueSkillCollisionLayer* l);

uint8_t rogue_animation_collision_evaluate_timeline(const RogueCollisionTimeline* tl, float time_ms,
                                                    uint8_t* out_indices, uint8_t max_indices)
{
    if (!tl)
        return 0;
    if (time_ms < 0.f)
        time_ms = 0.f;
    if (tl->loop_animation && tl->total_cycle_time_ms > 0.f)
    {
        /* Wrap using simple modulo (avoid fmod precision for huge values by manual subtraction). */
        while (time_ms >= tl->total_cycle_time_ms)
            time_ms -= tl->total_cycle_time_ms;
    }
    uint8_t active_count = 0;
    for (uint8_t i = 0; i < tl->window_count; ++i)
    {
        const RogueCollisionTimelineWindow* w = &tl->windows[i];
        if (w->duration_ms <= 0.f)
            continue; /* ignore degenerate */
        float end = w->timestamp_ms + w->duration_ms;
        if (time_ms < w->timestamp_ms || time_ms > end)
            continue;
        if (out_indices && active_count < max_indices)
            out_indices[active_count] = i;
        active_count++;
    }
    return active_count;
}

uint8_t rogue_animation_collision_evaluate_timeline_scaled(const RogueAnimationCollisionSync* sync,
                                                           const RogueCollisionTimeline* tl,
                                                           float time_ms, uint8_t* out_indices,
                                                           uint8_t max_indices)
{
    float speed = 1.f;
    if (sync && sync->playback_speed > 0.f)
        speed = sync->playback_speed;
    /* Scale time down by speed (faster playback means reaching later timeline positions earlier).
     */
    float scaled_time = time_ms * speed;
    return rogue_animation_collision_evaluate_timeline(tl, scaled_time, out_indices, max_indices);
}

/* Helper: detect if any window boundary (start or end) lies in (a,b] given potential wrap. */
static int timeline_has_boundary_between(const RogueCollisionTimeline* tl, float a, float b)
{
    if (!tl)
        return 0;
    if (!tl->loop_animation || tl->total_cycle_time_ms <= 0.f)
    {
        if (b < a)
        {
            float tmp = a;
            a = b;
            b = tmp;
        }
        for (uint8_t i = 0; i < tl->window_count; ++i)
        {
            const RogueCollisionTimelineWindow* w = &tl->windows[i];
            if (w->duration_ms <= 0.f)
                continue;
            float start = w->timestamp_ms;
            float end = start + w->duration_ms;
            if ((start > a && start <= b) || (end > a && end <= b))
                return 1;
        }
        return 0;
    }
    float cycle = tl->total_cycle_time_ms;
    if (cycle <= 0.f)
        return 0;
    /* Normalize */
    while (a >= cycle)
        a -= cycle;
    while (b >= cycle)
        b -= cycle;
    int wrapped = (b < a);
    for (uint8_t i = 0; i < tl->window_count; ++i)
    {
        const RogueCollisionTimelineWindow* w = &tl->windows[i];
        if (w->duration_ms <= 0.f)
            continue;
        float start = w->timestamp_ms;
        float end = start + w->duration_ms;
        if (!wrapped)
        {
            if ((start > a && start <= b) || (end > a && end <= b))
                return 1;
        }
        else
        {
            if (((start > a && start < cycle) || (start >= 0.f && start <= b)) ||
                ((end > a && end < cycle) || (end >= 0.f && end <= b)))
                return 1;
        }
    }
    return 0;
}

uint8_t rogue_animation_collision_evaluate_timeline_cached(
    const RogueAnimationCollisionSync* sync, const RogueCollisionTimeline* tl, float time_ms,
    RogueAnimationCollisionEvalState* state, uint8_t* out_indices, uint8_t max_indices)
{
    if (!tl)
        return 0;
    float threshold = (sync ? (float) sync->frame_skip_threshold : 0.f);
    if (!state || threshold <= 0.f || !state->initialized)
    {
        uint8_t count = rogue_animation_collision_evaluate_timeline_scaled(
            sync, tl, time_ms, out_indices, max_indices);
        if (state)
        {
            state->initialized = 1;
            state->last_time_ms = time_ms;
            state->last_active_count = (count > 16 ? 16 : count);
            if (out_indices && count)
            {
                for (uint8_t i = 0; i < state->last_active_count; ++i)
                    state->last_active_indices[i] = out_indices[i];
            }
        }
        return count;
    }
    float dt = time_ms - state->last_time_ms;
    if (dt < 0.f)
        dt = -dt; /* handle time rewind conservatively (will re-evaluate) */
    if (dt <= threshold)
    {
        /* Check for boundary crossing; if none, reuse */
        float prev_t = state->last_time_ms;
        if (!timeline_has_boundary_between(tl, prev_t, time_ms))
        {
            uint8_t reuse = state->last_active_count;
            if (out_indices)
            {
                for (uint8_t i = 0; i < reuse && i < max_indices; ++i)
                    out_indices[i] = state->last_active_indices[i];
            }
            return (reuse > max_indices) ? max_indices : reuse;
        }
    }
    uint8_t new_count = rogue_animation_collision_evaluate_timeline_scaled(
        sync, tl, time_ms, out_indices, max_indices);
    state->last_time_ms = time_ms;
    state->last_active_count = (new_count > 16 ? 16 : new_count);
    if (out_indices && new_count)
    {
        for (uint8_t i = 0; i < state->last_active_count; ++i)
            state->last_active_indices[i] = out_indices[i];
    }
    return new_count;
}

int rogue_animation_collision_interpolate_masks(const RogueAnimationCollisionSync* sync,
                                                float time_ms,
                                                const struct RogueHitPixelMaskFrame** out_a,
                                                const struct RogueHitPixelMaskFrame** out_b,
                                                float* out_t)
{
    if (out_a)
        *out_a = NULL;
    if (out_b)
        *out_b = NULL;
    if (out_t)
        *out_t = 0.f;
    if (!sync || sync->keyframe_count == 0 || !sync->keyframe_timestamps)
        return 0;
    /* Apply playback speed scaling (keeping semantics consistent with scaled timeline eval):
     * faster playback_speed (>1) advances along keyframes more quickly for a given real time. */
    if (sync && sync->playback_speed > 0.f)
        time_ms *= sync->playback_speed;
    /* Clamp negative time */
    if (time_ms < 0.f)
        time_ms = 0.f;
    /* Bracketing search. Prefer binary search when timestamps are monotonic; otherwise fallback
     * to linear scan. */
    uint8_t last_index = sync->keyframe_count - 1;
    int monotonic = 1;
    for (uint8_t i = 1; i < sync->keyframe_count; ++i)
    {
        if (sync->keyframe_timestamps[i] < sync->keyframe_timestamps[i - 1])
        {
            monotonic = 0;
            break;
        }
    }
    /* If time beyond last timestamp -> clamp */
    float last_ts = sync->keyframe_timestamps[last_index];
    if (time_ms >= last_ts || !sync->smooth_interpolation || sync->keyframe_count == 1)
    {
        if (out_a)
        {
            /* Find greatest keyframe <= time */
            uint8_t k = 0;
            if (monotonic)
            {
                int lo = 0, hi = (int) last_index, ans = 0;
                while (lo <= hi)
                {
                    int mid = (lo + hi) >> 1;
                    float ts = sync->keyframe_timestamps[(uint8_t) mid];
                    if (ts <= time_ms)
                    {
                        ans = mid;
                        lo = mid + 1;
                    }
                    else
                    {
                        hi = mid - 1;
                    }
                }
                k = (uint8_t) ans;
            }
            else
            {
                for (uint8_t i = 0; i < sync->keyframe_count; ++i)
                {
                    if (sync->keyframe_timestamps[i] <= time_ms)
                        k = i;
                    else
                        break;
                }
            }
            if (sync->keyframe_masks)
                *out_a = sync->keyframe_masks[k];
        }
        return 1;
    }
    /* Identify bracketing keyframes (i,i+1) where ts[i] <= time < ts[i+1] */
    uint8_t base = 0;
    if (monotonic)
    {
        /* Find first index j such that ts[j] > time_ms, then base=j-1 (clamped >=0) */
        int lo = 0, hi = (int) last_index, ans = (int) last_index + 1;
        while (lo <= hi)
        {
            int mid = (lo + hi) >> 1;
            float ts = sync->keyframe_timestamps[(uint8_t) mid];
            if (ts > time_ms)
            {
                ans = mid;
                hi = mid - 1;
            }
            else
            {
                lo = mid + 1;
            }
        }
        int b = ans - 1;
        if (b < 0)
            b = 0;
        if (b > (int) last_index - 1)
            b = (int) last_index - 1;
        base = (uint8_t) b;
    }
    else
    {
        for (uint8_t i = 0; i < last_index; ++i)
        {
            float b_ts = sync->keyframe_timestamps[i + 1];
            if (time_ms < b_ts)
            {
                base = i;
                break;
            }
        }
    }
    float a_ts = sync->keyframe_timestamps[base];
    float b_ts = sync->keyframe_timestamps[base + 1];
    float denom = (b_ts - a_ts);
    float t = 0.f;
    if (denom > 0.f)
        t = (time_ms - a_ts) / denom;
    if (t < 0.f)
        t = 0.f;
    if (t > 1.f)
        t = 1.f;
    /* Quality modes:
       - interpolation_quality >= 0.9f: quintic smootherstep for even smoother easing
       - interpolation_quality >= 0.5f: cubic smoothstep
     */
    if (sync->interpolation_quality >= 0.9f)
    {
        float t2 = t * t;
        float t3 = t2 * t;
        float t4 = t3 * t;
        float t5 = t4 * t;
        /* smootherstep: 6t^5 - 15t^4 + 10t^3 */
        t = 6.f * t5 - 15.f * t4 + 10.f * t3;
    }
    else if (sync->interpolation_quality >= 0.5f)
    {
        t = t * t * (3.f - 2.f * t); /* smoothstep */
    }
    if (out_a && sync->keyframe_masks)
        *out_a = sync->keyframe_masks[base];
    if (out_b && sync->keyframe_masks)
        *out_b = sync->keyframe_masks[base + 1];
    if (out_t)
        *out_t = t; /* linear factor */
    return 1;
}

uint8_t rogue_animation_collision_timeline_events(const RogueCollisionTimeline* tl,
                                                  float prev_time_ms, float curr_time_ms,
                                                  RogueCollisionTimelineEvent* out_events,
                                                  uint8_t max_events)
{
    if (!tl || max_events == 0 || !out_events)
        return 0;
    if (prev_time_ms < 0.f)
        prev_time_ms = 0.f;
    if (curr_time_ms < 0.f)
        curr_time_ms = 0.f;
    uint8_t emitted = 0;
/* Helper to append an event */
#define ROGUE_EMIT(ev_type, w_index, ev_time)                                                      \
    do                                                                                             \
    {                                                                                              \
        if (emitted < max_events)                                                                  \
        {                                                                                          \
            out_events[emitted].type = (ev_type);                                                  \
            out_events[emitted].window_index = (uint8_t) (w_index);                                \
            out_events[emitted].event_time_ms = (ev_time);                                         \
        }                                                                                          \
        emitted++;                                                                                 \
    } while (0)

    if (!tl->loop_animation || tl->total_cycle_time_ms <= 0.f)
    {
        if (curr_time_ms < prev_time_ms)
        {
            float tmp = prev_time_ms; /* swap to enforce ascending */
            prev_time_ms = curr_time_ms;
            curr_time_ms = tmp;
        }
        for (uint8_t i = 0; i < tl->window_count; ++i)
        {
            const RogueCollisionTimelineWindow* w = &tl->windows[i];
            if (w->duration_ms <= 0.f)
                continue;
            float start = w->timestamp_ms;
            float end = start + w->duration_ms;
            if (start > curr_time_ms || end < prev_time_ms)
                continue; /* no overlap with interval */
            /* ENTER when start in (prev_time_ms, curr_time_ms] */
            if (start > prev_time_ms && start <= curr_time_ms)
                ROGUE_EMIT(ROGUE_COLLISION_WINDOW_ENTER, i, start);
            /* EXIT when end in (prev_time_ms, curr_time_ms] */
            if (end > prev_time_ms && end <= curr_time_ms)
                ROGUE_EMIT(ROGUE_COLLISION_WINDOW_EXIT, i, end);
        }
    }
    else
    {
        float cycle = tl->total_cycle_time_ms;
        if (cycle <= 0.f)
            return 0;
        /* Normalize times inside [0,cycle) while capturing wrap */
        /* We assume frame deltas so |curr-prev| < 2*cycle. */
        int wrapped = 0;
        float p = prev_time_ms;
        float c = curr_time_ms;
        if (p >= cycle)
        {
            p = (float) ((int) (p) % (int) (cycle));
        }
        if (c >= cycle)
        {
            c = (float) ((int) (c) % (int) (cycle));
        }
        if (c < p)
            wrapped = 1; /* crossed cycle boundary */
        for (uint8_t i = 0; i < tl->window_count; ++i)
        {
            const RogueCollisionTimelineWindow* w = &tl->windows[i];
            if (w->duration_ms <= 0.f)
                continue;
            float start = w->timestamp_ms;
            float end = start + w->duration_ms;
            /* Check both segments when wrapped: (p,cycle] U [0,c] */
            if (!wrapped)
            {
                if (start > p && start <= c)
                    ROGUE_EMIT(ROGUE_COLLISION_WINDOW_ENTER, i, start);
                if (end > p && end <= c)
                    ROGUE_EMIT(ROGUE_COLLISION_WINDOW_EXIT, i, end);
            }
            else
            {
                if ((start > p && start < cycle) || (start >= 0.f && start <= c))
                    ROGUE_EMIT(ROGUE_COLLISION_WINDOW_ENTER, i, start);
                if ((end > p && end < cycle) || (end >= 0.f && end <= c))
                    ROGUE_EMIT(ROGUE_COLLISION_WINDOW_EXIT, i, end);
            }
        }
    }
    /* Ensure events are in chronological order and ENTER before EXIT on ties. Since we appended
       in timestamp order per window, a lightweight stable insertion sort across the buffer is
       sufficient for the small event counts typical here. */
    uint8_t count_out = (emitted > max_events) ? max_events : emitted;
    for (uint8_t i = 1; i < count_out; ++i)
    {
        RogueCollisionTimelineEvent key = out_events[i];
        int j = (int) i - 1;
        while (j >= 0)
        {
            const RogueCollisionTimelineEvent* e = &out_events[j];
            if (e->event_time_ms > key.event_time_ms ||
                (e->event_time_ms == key.event_time_ms && e->type > key.type))
            {
                out_events[j + 1] = out_events[j];
                --j;
            }
            else
            {
                break;
            }
        }
        out_events[j + 1] = key;
    }
    return count_out;
#undef ROGUE_EMIT
}

uint8_t rogue_animation_collision_timeline_events_scaled(const RogueAnimationCollisionSync* sync,
                                                         const RogueCollisionTimeline* tl,
                                                         float prev_time_ms, float curr_time_ms,
                                                         RogueCollisionTimelineEvent* out_events,
                                                         uint8_t max_events)
{
    float speed = 1.f;
    if (sync && sync->playback_speed > 0.f)
        speed = sync->playback_speed;
    float p = prev_time_ms * speed;
    float c = curr_time_ms * speed;
    return rogue_animation_collision_timeline_events(tl, p, c, out_events, max_events);
}

int rogue_animation_collision_interpolate_from_skill_layer(
    const RogueAnimationCollisionSync* sync, const struct RogueSkillCollisionLayer* layer,
    const struct RogueHitPixelMaskFrame** out_a, const struct RogueHitPixelMaskFrame** out_b,
    float* out_t)
{
    if (out_a)
        *out_a = NULL;
    if (out_b)
        *out_b = NULL;
    if (out_t)
        *out_t = 0.f;
    if (!sync || !layer || !sync->keyframe_masks || sync->keyframe_count == 0)
        return 0;
    /* Derive fractional frame index externally (layer API). We do NOT depend on layer internals
     * besides frame_count semantics. For now we treat keyframes == frames for mapping. */
    float idx = rogue_skill_collision_layer_frame_index(layer);
    if (idx < 0.f)
        idx = 0.f;
    float max_index = (float) (sync->keyframe_count - 1);
    if (idx >= max_index || !sync->smooth_interpolation)
    {
        int base = (idx >= max_index) ? (int) max_index : (int) idx;
        if (base < 0)
            base = 0;
        if (out_a)
            *out_a = sync->keyframe_masks[base];
        return 1;
    }
    int base = (int) idx;
    float frac = idx - (float) base;
    if (base < 0)
        base = 0;
    if (out_a)
        *out_a = sync->keyframe_masks[base];
    if (out_b)
        *out_b = sync->keyframe_masks[base + 1];
    if (out_t)
        *out_t = (frac < 0.f) ? 0.f : (frac > 1.f ? 1.f : frac);
    return 1;
}

/* Future extensions (deferred):
 *  - rogue_animation_collision_evaluate_timeline_ex: multi-speed scaling & event queue emission
 *  - Mask interpolation quality modes (cubic, Hermite, distance-field based blending)
 *  - Adaptive frame skip: monitor evaluation cost & dynamically widen sampling interval
 */

/* Internal helper: ensure scratch blended frame capacity (alloc or resize). */
static struct RogueHitPixelMaskFrame* ensure_blended_capacity(RogueAnimationCollisionSync* sync,
                                                              int w, int h)
{
    if (!sync)
        return NULL;
    if (sync->blended_scratch && (sync->blended_w != w || sync->blended_h != h))
    {
        /* Return to pool or free before reacquiring */
        if (sync->blended_from_pool)
            pool_release(sync->blended_scratch);
        else
        {
            free(sync->blended_scratch->bits);
            free(sync->blended_scratch);
        }
        sync->blended_scratch = NULL;
        sync->blended_from_pool = false;
    }
    if (!sync->blended_scratch)
    {
        /* Try pooled acquire first */
        struct RogueHitPixelMaskFrame* pooled = pool_acquire(w, h);
        if (pooled)
        {
            sync->blended_scratch = pooled;
            sync->blended_from_pool = true;
        }
        else
        {
            /* Fallback to heap alloc if pool exhausted */
            sync->blended_scratch =
                (struct RogueHitPixelMaskFrame*) calloc(1, sizeof(*sync->blended_scratch));
            if (!sync->blended_scratch)
                return NULL;
            int pitch_words = (w + 31) / 32;
            sync->blended_scratch->bits =
                (uint32_t*) calloc((size_t) pitch_words * (size_t) h, sizeof(uint32_t));
            if (!sync->blended_scratch->bits)
            {
                free(sync->blended_scratch);
                sync->blended_scratch = NULL;
                return NULL;
            }
            sync->blended_scratch->width = w;
            sync->blended_scratch->height = h;
            sync->blended_scratch->pitch_words = pitch_words;
            sync->blended_scratch->origin_x = 0;
            sync->blended_scratch->origin_y = 0;
            sync->blended_from_pool = false;
        }
        sync->blended_w = w;
        sync->blended_h = h;
    }
    return sync->blended_scratch;
}

const struct RogueHitPixelMaskFrame*
rogue_animation_collision_morph_mask(RogueAnimationCollisionSync* sync, float time_ms)
{
    if (!sync)
        return NULL;
    const struct RogueHitPixelMaskFrame *a = NULL, *b = NULL;
    float t = 0.f;
    if (!rogue_animation_collision_interpolate_masks(sync, time_ms, &a, &b, &t) || !a)
        return NULL;
    if (!b || !sync->smooth_interpolation || sync->keyframe_count < 2)
        return a; /* nothing to blend */
    /* Endpoint fast paths */
    if (t <= 0.15f)
        return a;
    if (t >= 0.85f)
        return b;
    /* Dimension check */
    if (a->width != b->width || a->height != b->height || a->pitch_words != b->pitch_words)
        return a; /* future: resample path */
    struct RogueHitPixelMaskFrame* blend = ensure_blended_capacity(sync, a->width, a->height);
    if (!blend)
        return a;
    /* Union OR conservative blend. */
    size_t words = (size_t) a->pitch_words * (size_t) a->height;
    for (size_t i = 0; i < words; ++i)
        blend->bits[i] = a->bits[i] | b->bits[i];
    blend->origin_x = a->origin_x; /* carry through */
    blend->origin_y = a->origin_y;
    return blend;
}

void rogue_animation_collision_sync_release(RogueAnimationCollisionSync* sync)
{
    if (!sync)
        return;
    if (sync->blended_scratch)
    {
        if (sync->blended_from_pool)
            pool_release(sync->blended_scratch);
        else
        {
            free(sync->blended_scratch->bits);
            free(sync->blended_scratch);
        }
        sync->blended_scratch = NULL;
        sync->blended_w = sync->blended_h = 0;
        sync->blended_from_pool = false;
    }
}
