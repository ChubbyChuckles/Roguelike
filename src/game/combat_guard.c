#include "../core/equipment/equipment_procs.h" /* for reactive shield procs */
#include "combat.h"
#include "combat_internal.h"
#include "stat_cache.h"
#include <math.h>
#include <stdlib.h>

/* Guard & Poise / Incoming Melee */
static void rogue_player_face(RoguePlayer* p, int dir)
{
    if (!p)
        return;
    if (dir < 0 || dir > 3)
        return;
    p->facing = dir;
}
static void rogue_player_facing_dir(const RoguePlayer* p, float* dx, float* dy)
{
    switch (p->facing)
    {
    case 0:
        *dx = 0;
        *dy = 1;
        break;
    case 1:
        *dx = -1;
        *dy = 0;
        break;
    case 2:
        *dx = 1;
        *dy = 0;
        break;
    case 3:
        *dx = 0;
        *dy = -1;
        break;
    default:
        *dx = 0;
        *dy = 1;
        break;
    }
}

int rogue_player_begin_guard(RoguePlayer* p, int guard_dir)
{
    if (!p)
        return 0;
    if (p->guard_meter <= 0.0f)
    {
        p->guarding = 0;
        return 0;
    }
    p->guarding = 1;
    p->guard_active_time_ms = 0.0f;
    rogue_player_face(p, guard_dir);
    return 1;
}
int rogue_player_update_guard(RoguePlayer* p, float dt_ms)
{
    if (!p)
        return 0;
    int chip = 0;
    extern struct RogueStatCache g_player_stat_cache;
    float rec_mult = 1.0f + (g_player_stat_cache.guard_recovery_pct / 100.0f);
    if (rec_mult < 0.10f)
        rec_mult = 0.10f; /* prevent negative / zero */
    if (rec_mult > 3.0f)
        rec_mult = 3.0f;
    if (p->guarding)
    {
        p->guard_active_time_ms += dt_ms; /* drain is inversely affected by recovery modifier
                                             (better recovery => less drain) */
        float drain_mult = 1.0f - (g_player_stat_cache.guard_recovery_pct / 150.0f);
        if (drain_mult < 0.25f)
            drain_mult = 0.25f;
        p->guard_meter -= dt_ms * ROGUE_GUARD_METER_DRAIN_HOLD_PER_MS * drain_mult;
        if (p->guard_meter <= 0.0f)
        {
            p->guard_meter = 0.0f;
            p->guarding = 0;
        }
    }
    else
    {
        p->guard_meter += dt_ms * ROGUE_GUARD_METER_RECOVER_PER_MS * rec_mult;
        if (p->guard_meter > p->guard_meter_max)
            p->guard_meter = p->guard_meter_max;
    }
    rogue_player_poise_regen_tick(p, dt_ms);
    return chip;
}

int rogue_player_apply_incoming_melee(RoguePlayer* p, float raw_damage, float attack_dir_x,
                                      float attack_dir_y, int poise_damage, int* out_blocked,
                                      int* out_perfect)
{
    if (out_blocked)
        *out_blocked = 0;
    if (out_perfect)
        *out_perfect = 0;
    if (!p)
        return (int) raw_damage;
    if (p->iframes_ms > 0.0f)
        return 0;
    if (raw_damage < 0)
        raw_damage = 0;
    float fdx, fdy;
    rogue_player_facing_dir(p, &fdx, &fdy);
    float alen = sqrtf(attack_dir_x * attack_dir_x + attack_dir_y * attack_dir_y);
    if (alen > 0.0001f)
    {
        attack_dir_x /= alen;
        attack_dir_y /= alen;
    }
    float dot = fdx * attack_dir_x + fdy * attack_dir_y;
    int blocked = 0;
    int perfect = 0;
    /* Phase 7.1 passive block chance (independent of guarding) */
    extern struct RogueStatCache g_player_stat_cache; /* use aggregated defensive stats */
    int passive_block = 0;
    if (g_player_stat_cache.block_chance > 0)
    {
        int roll = rand() % 100;
        if (roll < g_player_stat_cache.block_chance)
        {
            passive_block = 1;
        }
    }
    if (p->guarding && p->guard_meter > 0.0f && dot >= ROGUE_GUARD_CONE_DOT)
    {
        blocked = 1;
        perfect = (p->guard_active_time_ms <= p->perfect_guard_window_ms) ? 1 : 0;
        float chip = raw_damage * ROGUE_GUARD_CHIP_PCT;
        if (chip < 1.0f)
            chip = (raw_damage > 0) ? 1.0f : 0.0f;
        if (perfect)
        {
            chip = 0.0f;
            p->guard_meter += ROGUE_PERFECT_GUARD_REFUND;
            if (p->guard_meter > p->guard_meter_max)
                p->guard_meter = p->guard_meter_max;
            p->poise += ROGUE_PERFECT_GUARD_POISE_BONUS;
            if (p->poise > p->poise_max)
                p->poise = p->poise_max;
        }
        else
        {
            p->guard_meter -= ROGUE_GUARD_METER_DRAIN_ON_BLOCK;
            if (p->guard_meter < 0.0f)
                p->guard_meter = 0.0f;
            if (poise_damage > 0)
            {
                float pd = (float) poise_damage * ROGUE_GUARD_BLOCK_POISE_SCALE;
                p->poise -= pd;
                if (p->poise < 0.0f)
                    p->poise = 0.0f;
                p->poise_regen_delay_ms = ROGUE_POISE_REGEN_DELAY_AFTER_HIT;
            }
        }
        rogue_procs_event_block(); /* trigger potential reactive shield proc */
        /* Apply reactive shield absorb to post-guard chip damage before returning. */
        if (chip > 0.0f)
        {
            int absorb_pool = rogue_procs_absorb_pool();
            if (absorb_pool > 0)
            {
                int consumed = rogue_procs_consume_absorb((int) chip);
                chip -= (float) consumed;
                if (chip < 0.0f)
                    chip = 0.0f;
            }
        }
        if (out_blocked)
            *out_blocked = 1;
        if (perfect && out_perfect)
            *out_perfect = 1;
        return (int) chip;
    }
    if (passive_block)
    {
        blocked = 1; /* Flat reduction by block_value; never less than 0 */
        int red = g_player_stat_cache.block_value;
        if (red < 0)
            red = 0;
        raw_damage -= (float) red;
        if (raw_damage < 0)
            raw_damage = 0;
        rogue_procs_event_block(); /* passive block also triggers block procs */
        /* Apply reactive shield absorb to post-block damage before returning. */
        if (raw_damage > 0)
        {
            int absorb_pool = rogue_procs_absorb_pool();
            if (absorb_pool > 0)
            {
                int consumed = rogue_procs_consume_absorb((int) raw_damage);
                raw_damage -= (float) consumed;
                if (raw_damage < 0)
                    raw_damage = 0;
            }
        }
        if (out_blocked)
            *out_blocked = 1;
        return (int) raw_damage;
    }
    int triggered_reaction = 0;
    if (poise_damage > 0 && !_rogue_player_is_hyper_armor_active())
    {
        float before = p->poise;
        p->poise -= (float) poise_damage;
        if (p->poise < 0.0f)
            p->poise = 0.0f;
        if (before > 0.0f && p->poise <= 0.0f)
        {
            rogue_player_apply_reaction(p, 2);
            triggered_reaction = 1;
        }
    }
    if (!triggered_reaction)
    {
        if (raw_damage >= 80)
        {
            rogue_player_apply_reaction(p, 3);
        }
        else if (raw_damage >= 25)
        {
            rogue_player_apply_reaction(p, 1);
        }
    }
    p->poise_regen_delay_ms = ROGUE_POISE_REGEN_DELAY_AFTER_HIT;
    /* Phase 7.2 damage conversion (physical -> elemental) implementation.
        Since this function currently receives only a single raw_damage assumed physical, we
       internally partition it. Ordering: passive block -> guard block -> conversion -> (future
       elemental mitigation when player resist fields added). We expose converted components via
       local variables for potential future telemetry (unused now). */
    float remain_phys = raw_damage;
    if (remain_phys < 0)
        remain_phys = 0;
    int c_fire = g_player_stat_cache.phys_conv_fire_pct;
    if (c_fire < 0)
        c_fire = 0;
    int c_frost = g_player_stat_cache.phys_conv_frost_pct;
    if (c_frost < 0)
        c_frost = 0;
    int c_arc = g_player_stat_cache.phys_conv_arcane_pct;
    if (c_arc < 0)
        c_arc = 0;
    int total_conv = c_fire + c_frost + c_arc;
    if (total_conv > 95)
        total_conv = 95; /* retain at least 5% physical identity */
    float fire_amt = 0, frost_amt = 0, arc_amt = 0;
    if (total_conv > 0 && remain_phys > 0)
    {
        fire_amt = remain_phys * ((float) c_fire / 100.0f);
        frost_amt = remain_phys * ((float) c_frost / 100.0f);
        arc_amt = remain_phys * ((float) c_arc / 100.0f);
        float sum = fire_amt + frost_amt + arc_amt;
        if (sum > remain_phys)
        {
            float scale = remain_phys / sum;
            fire_amt *= scale;
            frost_amt *= scale;
            arc_amt *= scale;
        }
        remain_phys -= (fire_amt + frost_amt + arc_amt);
    }
    /* (Future) Apply elemental resistances once player stat cache exposes them (currently only
     * enemy has). */
    raw_damage = remain_phys + fire_amt + frost_amt + arc_amt; /* conservation */
    /* Phase 7.4 reactive shield absorb: consume before reflect. */
    int absorb_pool = rogue_procs_absorb_pool();
    if (absorb_pool > 0 && raw_damage > 0)
    {
        int consumed = rogue_procs_consume_absorb((int) raw_damage);
        raw_damage -= (float) consumed;
        if (raw_damage < 0)
            raw_damage = 0;
    }
    /* Phase 7.5 thorns reflect: reflect percent of final post-conversion damage; cap applies. */
    if (g_player_stat_cache.thorns_percent > 0 && raw_damage > 0)
    {
        int reflect = (int) ((raw_damage * g_player_stat_cache.thorns_percent) / 100.0f);
        if (g_player_stat_cache.thorns_cap > 0 && reflect > g_player_stat_cache.thorns_cap)
            reflect = g_player_stat_cache.thorns_cap; /* TODO: hook into enemy damage pipeline when
                                                         attacker context available */
        (void) reflect;
    }
    return (int) raw_damage;
}

void rogue_player_poise_regen_tick(RoguePlayer* p, float dt_ms)
{
    if (!p)
        return;
    if (p->poise_regen_delay_ms > 0)
    {
        p->poise_regen_delay_ms -= dt_ms;
        if (p->poise_regen_delay_ms < 0)
            p->poise_regen_delay_ms = 0;
    }
    if (p->poise_regen_delay_ms <= 0 && p->poise < p->poise_max)
    {
        float missing = p->poise_max - p->poise;
        float ratio = missing / p->poise_max;
        if (ratio < 0)
            ratio = 0;
        if (ratio > 1)
            ratio = 1;
        float regen = (ROGUE_POISE_REGEN_BASE_PER_MS * dt_ms) * (1.0f + 1.75f * ratio * ratio);
        p->poise += regen;
        if (p->poise > p->poise_max)
            p->poise = p->poise_max;
    }
}
