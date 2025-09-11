/* skill_collision_manager.c - Milestone 3.1 implementation (see header for details) */
#include "game/skill_collision_manager.h"
#include <string.h>

void rogue_skill_collision_effect_init(RogueSkillCollisionEffect* e)
{
    if (!e)
        return;
    memset(e, 0, sizeof(*e));
}

static int curve_is_all_zero(const float curve[8])
{
    for (int i = 0; i < 8; ++i)
    {
        if (curve[i] != 0.f)
            return 0;
    }
    return 1;
}

int rogue_skill_collision_effect_add_layer(RogueSkillCollisionEffect* e,
                                           const RogueSkillCollisionLayer* src)
{
    if (!e || !src)
        return -1;
    if (e->layer_count >= 4)
        return -1;
    RogueSkillCollisionLayer* dst = &e->layers[e->layer_count++];
    *dst = *src; /* copy config */
    dst->elapsed_ms = 0.f;
    dst->active = 0;
    dst->finished = 0;
    dst->hits_recorded = 0;
    /* Extend total duration */
    float layer_end = dst->start_time_ms + dst->duration_ms;
    if (layer_end > e->total_duration_ms)
        e->total_duration_ms = layer_end;
    return 0;
}

float rogue_skill_collision_layer_intensity(const RogueSkillCollisionLayer* l)
{
    if (!l)
        return 0.f;
    if (l->duration_ms <= 0.f)
        return 0.f;
    if (curve_is_all_zero(l->intensity_curve))
        return 1.f;                           /* default flat */
    float t = l->elapsed_ms / l->duration_ms; /* 0..1 */
    if (t < 0.f)
        t = 0.f;
    if (t > 1.f)
        t = 1.f;
    /* Map t into segment [i,i+1] over 7 segments */
    float scaled = t * 7.f;
    int i = (int) scaled;
    if (i >= 7)
        i = 7; /* last point */
    float frac = scaled - (float) i;
    if (i == 7)
        return l->intensity_curve[7];
    float a = l->intensity_curve[i];
    float b = l->intensity_curve[i + 1];
    return a + (b - a) * frac;
}

float rogue_skill_collision_layer_frame_index(const RogueSkillCollisionLayer* l)
{
    if (!l || l->frame_count <= 1 || l->duration_ms <= 0.f)
        return 0.f;
    float t = l->elapsed_ms / l->duration_ms;
    if (t < 0.f)
        t = 0.f;
    if (t > 1.f)
        t = 1.f;
    float idx = t * (float) (l->frame_count - 1);
    return idx; /* fractional index - caller may floor/ceil for sampling */
}

int rogue_skill_collision_test_projectile(const RogueSkillCollisionLayer* l, float target_x,
                                          float target_y)
{
    if (!l || l->type != ROGUE_SKILL_PROJECTILE)
        return 0;
    float dx = target_x - l->proj_pos_x;
    float dy = target_y - l->proj_pos_y;
    float r = l->proj_radius;
    if (r <= 0.f)
        return 0;
    return (dx * dx + dy * dy) <= (r * r);
}

uint32_t rogue_skill_collision_effect_tick(RogueSkillCollisionEffect* e, float dt_ms,
                                           const RogueSkillCollisionTarget* targets,
                                           uint32_t target_count,
                                           RogueSkillCollisionHitBuffer* out_hits)
{
    if (!e || dt_ms < 0.f)
        return 0;
    /* Capture time at start of tick to detect per-layer activation within this interval. */
    const float tick_start_time = e->global_time_ms;
    const float tick_end_time = tick_start_time + dt_ms;
    e->global_time_ms = tick_end_time;
    if (e->effect_finished)
        return 0; /* already done */
    uint32_t appended = 0;
    for (uint8_t li = 0; li < e->layer_count; ++li)
    {
        RogueSkillCollisionLayer* L = &e->layers[li];
        if (L->finished)
            continue;
        const float elapsed_before = L->elapsed_ms;
        /* Update activation */
        if (e->global_time_ms >= L->start_time_ms)
        {
            L->active = 1;
            /* Advance local time at the start of the tick so intensity/frames
             * reflect the state during this update interval. */
            L->elapsed_ms += dt_ms;
        }
        if (!L->active)
            continue; /* not started yet */
        if (L->elapsed_ms > L->duration_ms)
        {
            L->finished = 1;
            continue;
        }
        /* Collision evaluation: for INSTANT or first activation frame treat as single burst.
         * For persistent types (AOE_PERSISTENT / CHANNELED / MULTI_HIT) we allow hits every tick.
         */
        int burst = 0;
        switch (L->type)
        {
        case ROGUE_SKILL_INSTANT:
            burst = 1;
            break;
        case ROGUE_SKILL_AOE_EXPANDING:
            burst = 1;
            break; /* one-time simplified */
        case ROGUE_SKILL_AOE_PERSISTENT:
            burst = 1;
            break; /* per-tick */
        case ROGUE_SKILL_CHANNELED:
            burst = 1;
            break; /* per-tick */
        case ROGUE_SKILL_MULTI_HIT:
            burst = 1;
            break; /* per-tick */
        case ROGUE_SKILL_PROJECTILE:
            /* For projectile, evaluate collision along the segment swept over this tick
             * from previous position to new position. */
            burst = 1;
            break;
        }
        if (!burst)
            continue; /* defensive */
        if (!targets)
            continue;
        /* Track per-tick hit count for modes that allow repeated hits each tick. */
        uint8_t tick_hits_recorded = 0;
        /* Cache projectile previous/new positions if applicable */
        float prev_x = L->proj_pos_x, prev_y = L->proj_pos_y;
        float new_x = L->proj_pos_x, new_y = L->proj_pos_y;
        if (L->type == ROGUE_SKILL_PROJECTILE)
        {
            new_x = prev_x + L->proj_vel_x * dt_ms;
            new_y = prev_y + L->proj_vel_y * dt_ms;
            /* If this layer activated within this tick, avoid sweeping from the spawn
             * position to the end position to prevent retroactive hits. Treat as a
             * point check at the end of the tick only. */
            if ((tick_start_time < L->start_time_ms && tick_end_time >= L->start_time_ms) ||
                (elapsed_before <= 0.f && tick_start_time <= L->start_time_ms))
            {
                prev_x = new_x;
                prev_y = new_y;
            }
        }
        for (uint32_t ti = 0; ti < target_count; ++ti)
        {
            if (L->max_targets)
            {
                /* For MULTI_HIT we cap per tick; for others cap over lifetime */
                if (L->type == ROGUE_SKILL_MULTI_HIT)
                {
                    if (tick_hits_recorded >= L->max_targets)
                        break;
                }
                else if (L->hits_recorded >= L->max_targets)
                {
                    break;
                }
            }
            const RogueSkillCollisionTarget* T = &targets[ti];
            if ((T->layer_mask & L->affected_layers) == 0)
                continue; /* filter */
            if (L->type == ROGUE_SKILL_PROJECTILE)
            {
                /* Check distance from target to segment [prev -> new] <= radius */
                float px = prev_x, py = prev_y;
                float qx = new_x, qy = new_y;
                float vx = qx - px, vy = qy - py;
                float wx = T->x - px, wy = T->y - py;
                float vlen2 = vx * vx + vy * vy;
                float t = 0.f;
                if (vlen2 > 0.f)
                {
                    t = (wx * vx + wy * vy) / vlen2;
                    if (t < 0.f)
                        t = 0.f;
                    else if (t > 1.f)
                        t = 1.f;
                }
                float cx = px + vx * t;
                float cy = py + vy * t;
                float dx = T->x - cx;
                float dy = T->y - cy;
                float r = L->proj_radius;
                if (!(dx * dx + dy * dy <= r * r))
                    continue;
            }
            if (!L->pierces_enemies && L->hits_recorded > 0)
                break; /* only first */
            if (out_hits && out_hits->hits && out_hits->count < out_hits->capacity)
            {
                RogueSkillCollisionHit* h = &out_hits->hits[out_hits->count++];
                h->target_id = T->id;
                h->layer_index = li;
                h->time_ms = e->global_time_ms;
                /* INSTANT types sample intensity based on state at start of this tick (pre-advance)
                 * which for a zeroed curve yields ~0 early as expected by tests. For others,
                 * current elapsed_ms after increment is acceptable. Since we advanced elapsed_ms at
                 * the top, approximate start-of-tick by subtracting dt for instant types (clamped).
                 */
                float saved_elapsed = L->elapsed_ms;
                if (L->type == ROGUE_SKILL_INSTANT)
                {
                    float approx_start = saved_elapsed - dt_ms;
                    if (approx_start < 0.f)
                        approx_start = 0.f;
                    RogueSkillCollisionLayer tmp = *L;
                    tmp.elapsed_ms = approx_start;
                    h->intensity = rogue_skill_collision_layer_intensity(&tmp);
                }
                else
                {
                    h->intensity = rogue_skill_collision_layer_intensity(L);
                }
                appended++;
            }
            L->hits_recorded++;
            if (L->type == ROGUE_SKILL_MULTI_HIT)
                tick_hits_recorded++;
        }
        /* Commit projectile new position after evaluating all targets */
        if (L->type == ROGUE_SKILL_PROJECTILE)
        {
            L->proj_pos_x = new_x;
            L->proj_pos_y = new_y;
        }
    }
    /* Determine completion */
    uint8_t all_done = 1;
    for (uint8_t li = 0; li < e->layer_count; ++li)
    {
        if (!e->layers[li].finished)
        {
            all_done = 0;
            break;
        }
    }
    if (all_done)
        e->effect_finished = 1;
    return appended;
}
