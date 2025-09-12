/* spatial_acceleration.h - Phase 4.1 (slice): lightweight temporal coherence predictor
 * This minimal module introduces a deterministic predictor that can suggest
 * skipping redundant collision checks for slowly separating pairs across frames.
 * It is intentionally conservative and off-by-default (not wired into the main
 * pipeline yet) to avoid behavior changes; used only by unit tests for now.
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueVec2
    {
        float x, y;
    } RogueVec2;

    typedef struct RogueTemporalCoherenceEntry
    {
        uint32_t pair_hash;            /* hash of sorted (idA,idB) */
        float last_sep2;               /* last frame separation^2 */
        RogueVec2 rel_vel;             /* last observed relative velocity */
        uint8_t was_colliding;         /* previous frame collision state */
        uint8_t skip_frames_remaining; /* advisory: frames to skip (small) */
    } RogueTemporalCoherenceEntry;

#define ROGUE_TEMP_COHERENCE_CAP 256
    typedef struct RogueTemporalCoherenceCache
    {
        RogueTemporalCoherenceEntry entries[ROGUE_TEMP_COHERENCE_CAP];
        uint16_t count;
        float sep_threshold_px; /* small-distance threshold guiding skip */
        uint32_t frames;        /* lifetime frames processed (stage invocations) */
        uint32_t predicts;      /* total predicted skips */
        uint32_t updates;       /* total updates */
        uint32_t pairs_touched; /* total pairs touched (for hit rate) */
        uint32_t skip_hist[4];  /* histogram of skip lengths (index=frames skipped; 3=3+) */
        /* Candidate set size stats per stage invocation */
        uint64_t candidates_sum;
        uint32_t candidates_min;
        uint32_t candidates_max;
    } RogueTemporalCoherenceCache;

    /* Initialize/reset cache with a separation threshold in pixels. */
    static inline void rogue_temporal_cache_init(RogueTemporalCoherenceCache* c,
                                                 float sep_thresh_px)
    {
        if (!c)
            return;
        c->count = 0;
        c->sep_threshold_px = sep_thresh_px;
        c->frames = c->predicts = c->updates = 0;
        c->pairs_touched = 0;
        for (int i = 0; i < 4; ++i)
            c->skip_hist[i] = 0;
        c->candidates_sum = 0;
        c->candidates_min = UINT32_MAX;
        c->candidates_max = 0;
    }

    /* Hash two 32-bit ids in a deterministic manner (order-insensitive). */
    static inline uint32_t rogue_pair_hash(uint32_t a, uint32_t b)
    {
        uint32_t lo = a < b ? a : b;
        uint32_t hi = a < b ? b : a;
        /* xorshift-mix */
        uint32_t h = lo * 0x9E3779B1u ^ (hi + 0x85EBCA6Bu);
        h ^= h >> 16;
        h *= 0x7FEB352Du;
        h ^= h >> 15;
        h *= 0x846CA68Bu;
        h ^= h >> 16;
        return h;
    }

    /* Update or insert a pair record; return index. */
    static inline int rogue_temporal_cache_touch(RogueTemporalCoherenceCache* c, uint32_t idA,
                                                 uint32_t idB, float sep2, RogueVec2 rel_vel,
                                                 int collided_now)
    {
        if (!c)
            return -1;
        uint32_t h = rogue_pair_hash(idA, idB);
        /* linear search (small cap) */
        for (uint16_t i = 0; i < c->count; ++i)
        {
            if (c->entries[i].pair_hash == h)
            {
                RogueTemporalCoherenceEntry* e = &c->entries[i];
                e->rel_vel = rel_vel;
                e->skip_frames_remaining = 0; /* clear on update; predictor sets */
                e->was_colliding = (uint8_t) (collided_now ? 1 : 0);
                e->last_sep2 = sep2;
                c->updates++;
                return (int) i;
            }
        }
        if (c->count >= ROGUE_TEMP_COHERENCE_CAP)
        {
            /* overwrite oldest (simple) */
            RogueTemporalCoherenceEntry* e = &c->entries[0];
            e->pair_hash = h;
            e->last_sep2 = sep2;
            e->rel_vel = rel_vel;
            e->was_colliding = (uint8_t) (collided_now ? 1 : 0);
            e->skip_frames_remaining = 0;
            c->updates++;
            return 0;
        }
        RogueTemporalCoherenceEntry* e = &c->entries[c->count];
        e->pair_hash = h;
        e->last_sep2 = sep2;
        e->rel_vel = rel_vel;
        e->was_colliding = (uint8_t) (collided_now ? 1 : 0);
        e->skip_frames_remaining = 0;
        c->updates++;
        return (int) c->count++;
    }

    /* Very conservative predictor: if pair was not colliding, separation is small but
     * increasing and relative speed is below threshold, advise skipping next frame. */
    static inline int rogue_temporal_cache_predict_skip(RogueTemporalCoherenceCache* c,
                                                        uint32_t idA, uint32_t idB, float new_sep2,
                                                        RogueVec2 new_rel_vel)
    {
        if (!c)
            return 0;
        uint32_t h = rogue_pair_hash(idA, idB);
        for (uint16_t i = 0; i < c->count; ++i)
        {
            RogueTemporalCoherenceEntry* e = &c->entries[i];
            if (e->pair_hash != h)
                continue;
            float thresh2 = c->sep_threshold_px * c->sep_threshold_px;
            float v2 = new_rel_vel.x * new_rel_vel.x + new_rel_vel.y * new_rel_vel.y;
            /* require: previously not colliding, separation increased but remains small,
               and relative speed small (<= threshold per frame unit). */
            if (!e->was_colliding && e->last_sep2 <= new_sep2 && new_sep2 <= thresh2 &&
                v2 <= thresh2)
            {
                e->skip_frames_remaining = 1;
                if (c->skip_hist[1] < UINT32_MAX)
                    c->skip_hist[1]++;
                c->predicts++;
                return 1;
            }
            return 0;
        }
        return 0;
    }

    /* Query whether a pair should be skipped this frame; if yes, decrement its counter. */
    static inline int rogue_temporal_cache_should_skip(RogueTemporalCoherenceCache* c, uint32_t idA,
                                                       uint32_t idB)
    {
        if (!c)
            return 0;
        uint32_t h = rogue_pair_hash(idA, idB);
        for (uint16_t i = 0; i < c->count; ++i)
        {
            RogueTemporalCoherenceEntry* e = &c->entries[i];
            if (e->pair_hash != h)
                continue;
            if (e->skip_frames_remaining > 0)
            {
                e->skip_frames_remaining--;
                return 1;
            }
            return 0;
        }
        return 0;
    }

#ifdef __cplusplus
}
#endif
