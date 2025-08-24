#include "combat.h"
#include <math.h>

/* Reactions & Directional Influence */
static void rogue_player_init_reaction_params(RoguePlayer* p)
{
    switch (p->reaction_type)
    {
    case 1:
        p->reaction_di_max = 0.35f;
        break;
    case 2:
        p->reaction_di_max = 0.55f;
        break;
    case 3:
        p->reaction_di_max = 0.85f;
        break;
    case 4:
        p->reaction_di_max = 1.00f;
        break;
    default:
        p->reaction_di_max = 0.0f;
        break;
    }
    p->reaction_di_accum_x = 0;
    p->reaction_di_accum_y = 0;
    p->reaction_canceled_early = 0;
}
void rogue_player_update_reactions(RoguePlayer* p, float dt_ms)
{
    if (!p)
        return;
    if (p->reaction_timer_ms > 0)
    {
        p->reaction_timer_ms -= dt_ms;
        if (p->reaction_timer_ms <= 0)
        {
            p->reaction_timer_ms = 0;
            p->reaction_type = 0;
            p->reaction_total_ms = 0;
            p->reaction_di_accum_x = 0;
            p->reaction_di_accum_y = 0;
            p->reaction_di_max = 0;
        }
    }
    if (p->iframes_ms > 0)
    {
        p->iframes_ms -= dt_ms;
        if (p->iframes_ms < 0)
            p->iframes_ms = 0;
    }
}
void rogue_player_apply_reaction(RoguePlayer* p, int reaction_type)
{
    if (!p)
        return;
    if (reaction_type <= 0)
        return;
    p->reaction_type = reaction_type;
    switch (reaction_type)
    {
    case 1:
        p->reaction_timer_ms = 220.0f;
        break;
    case 2:
        p->reaction_timer_ms = 600.0f;
        break;
    case 3:
        p->reaction_timer_ms = 900.0f;
        break;
    case 4:
        p->reaction_timer_ms = 1100.0f;
        break;
    default:
        p->reaction_timer_ms = 300.0f;
        break;
    }
    p->reaction_total_ms = p->reaction_timer_ms;
    rogue_player_init_reaction_params(p);
}
int rogue_player_try_reaction_cancel(RoguePlayer* p)
{
    if (!p)
        return 0;
    if (p->reaction_type == 0 || p->reaction_timer_ms <= 0)
        return 0;
    if (p->reaction_canceled_early)
        return 0;
    float min_frac = 0.0f, max_frac = 0.0f;
    switch (p->reaction_type)
    {
    case 1:
        min_frac = 0.40f;
        max_frac = 0.75f;
        break;
    case 2:
        min_frac = 0.55f;
        max_frac = 0.85f;
        break;
    case 3:
        min_frac = 0.60f;
        max_frac = 0.80f;
        break;
    case 4:
        min_frac = 0.65f;
        max_frac = 0.78f;
        break;
    default:
        return 0;
    }
    if (p->reaction_total_ms <= 0)
        return 0;
    float elapsed = p->reaction_total_ms - p->reaction_timer_ms;
    float frac = elapsed / p->reaction_total_ms;
    if (frac >= min_frac && frac <= max_frac)
    {
        p->reaction_timer_ms = 0;
        p->reaction_type = 0;
        p->reaction_canceled_early = 1;
        return 1;
    }
    return 0;
}
void rogue_player_apply_reaction_di(RoguePlayer* p, float dx, float dy)
{
    if (!p)
        return;
    if (p->reaction_type == 0 || p->reaction_timer_ms <= 0)
        return;
    if (p->reaction_di_max <= 0)
        return;
    float mag = sqrtf(dx * dx + dy * dy);
    if (mag > 1.0f && mag > 0)
    {
        dx /= mag;
        dy /= mag;
    }
    p->reaction_di_accum_x += dx * 0.08f;
    p->reaction_di_accum_y += dy * 0.08f;
    float acc_mag = sqrtf(p->reaction_di_accum_x * p->reaction_di_accum_x +
                          p->reaction_di_accum_y * p->reaction_di_accum_y);
    if (acc_mag > p->reaction_di_max && acc_mag > 0)
    {
        float scale = p->reaction_di_max / acc_mag;
        p->reaction_di_accum_x *= scale;
        p->reaction_di_accum_y *= scale;
    }
}
void rogue_player_add_iframes(RoguePlayer* p, float ms)
{
    if (!p)
        return;
    if (ms <= 0)
        return;
    if (p->iframes_ms < ms)
        p->iframes_ms = ms;
}
