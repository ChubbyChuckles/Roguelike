/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "entities/player.h"

void rogue_player_init(RoguePlayer* p)
{
    p->base.pos.x = 0.0f;
    p->base.pos.y = 0.0f;
    p->base.vel.x = 0.0f;
    p->base.vel.y = 0.0f;
    p->team_id = 0; /* player faction */
    p->health = 0; /* will be set after first recalc */
    p->max_health = 0;
    p->mana = 0;
    p->max_mana = 0;
    p->action_points = 0;
    p->max_action_points = 0;
    p->facing = 0;
    p->anim_time = 0.0f;
    p->anim_frame = 0;
    p->level = 1;
    p->xp = 0;
    p->xp_to_next = 20;
    p->strength = 5;
    p->dexterity = 5;
    p->vitality = 15; /* starting vitality */
    p->intelligence = 5;
    p->crit_chance = 0;
    p->crit_damage = 50; /* base 1.5x */
    /* Phase 2 baseline mitigation & penetration */
    p->armor = 0; p->resist_physical=0; p->resist_fire=0; p->resist_frost=0; p->resist_arcane=0; p->resist_bleed=0; p->resist_poison=0; p->pen_flat=0; p->pen_percent=0;
    /* Phase 3.1 new meters */
    p->guard_meter_max = 100.0f; p->guard_meter = p->guard_meter_max;
    p->poise_max = 60.0f; p->poise = p->poise_max;
    p->encumbrance_capacity = 50.0f; p->encumbrance = 0.0f; p->encumbrance_tier=0;
    /* Guard / parry fields */
    p->guarding = 0; p->guard_active_time_ms = 0.0f; p->perfect_guard_window_ms = 140.0f; /* first 140ms counts as perfect */
    p->poise_regen_delay_ms = 0.0f;
    /* Lock-on defaults */
    p->lock_on_active = 0; p->lock_on_target_index=-1; p->lock_on_radius=6.0f; p->lock_on_switch_cooldown_ms=0.0f;
    rogue_player_recalc_derived(p);
}

void rogue_player_recalc_derived(RoguePlayer* p)
{
    int old_max = p->max_health;
    /* New scaling: higher base + stronger vitality impact */
    p->max_health = 300 + p->vitality * 2 + (p->level-1) * 15; /* base 300 */
    /* Mana scaling: modest base, intelligence focus */
    int old_mmax = p->max_mana;
    p->max_mana = 50 + p->intelligence * 5 + (p->level-1) * 8;
    /* AP scaling: flat base + mild dexterity contribution */
    int old_apmax = p->max_action_points;
    p->max_action_points = 100 + p->dexterity * 2 + (p->level-1) * 3;
    if(p->health == 0 || p->health == old_max) p->health = p->max_health; /* fill to max on init or level-based rescale */
    if(p->health > p->max_health) p->health = p->max_health;
    if(p->mana == 0 || p->mana == old_mmax) p->mana = p->max_mana;
    if(p->mana > p->max_mana) p->mana = p->max_mana;
    if(p->action_points == 0 || p->action_points == old_apmax) p->action_points = p->max_action_points;
    if(p->action_points > p->max_action_points) p->action_points = p->max_action_points;
    if(p->crit_chance < 0) p->crit_chance = 0; if(p->crit_chance > 100) p->crit_chance = 100;
    if(p->crit_damage < 0) p->crit_damage = 0; if(p->crit_damage > 400) p->crit_damage = 400; /* cap 5x */
    /* Clamp new meters (recalc may later scale with stats/gear) */
    if(p->guard_meter > p->guard_meter_max) p->guard_meter = p->guard_meter_max;
    if(p->poise > p->poise_max) p->poise = p->poise_max;
    /* Recompute encumbrance tier (simple thresholds) */
    float ratio = (p->encumbrance_capacity>0)? (p->encumbrance / p->encumbrance_capacity) : 0.0f;
    if(ratio < 0.40f) p->encumbrance_tier = 0; else if(ratio < 0.70f) p->encumbrance_tier = 1; else if(ratio < 1.0f) p->encumbrance_tier = 2; else p->encumbrance_tier = 3;
}
