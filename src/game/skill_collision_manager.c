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
    if (target_count == 0 || !targets)
    {
        e->global_time_ms += dt_ms;
    }
    else
    {
        e->global_time_ms += dt_ms;
    }
    if (e->effect_finished)
        return 0; /* already done */
    uint32_t appended = 0;
    for (uint8_t li = 0; li < e->layer_count; ++li)
    {
        RogueSkillCollisionLayer* L = &e->layers[li];
        if (L->finished)
            continue;
        /* Update activation */
        if (e->global_time_ms >= L->start_time_ms)
        {
            L->active = 1;
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
            /* Advance projectile position before evaluating hits */
            L->proj_pos_x += L->proj_vel_x * dt_ms;
            L->proj_pos_y += L->proj_vel_y * dt_ms;
            burst = 1; /* treat projectile as per-tick evaluation */
            break;
        }
        if (!burst)
            continue; /* defensive */
        if (!targets)
            continue;
        for (uint32_t ti = 0; ti < target_count; ++ti)
        {
            if (L->max_targets && L->hits_recorded >= L->max_targets)
                break;
            const RogueSkillCollisionTarget* T = &targets[ti];
            if ((T->layer_mask & L->affected_layers) == 0)
                continue; /* filter */
            if (L->type == ROGUE_SKILL_PROJECTILE)
            {
                /* Require projectile radius overlap */
                if (!rogue_skill_collision_test_projectile(L, T->x, T->y))
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
                h->intensity = rogue_skill_collision_layer_intensity(L);
                appended++;
            }
            L->hits_recorded++;
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
