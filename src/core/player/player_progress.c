#include "player_progress.h"
#include "../../audio_vfx/effects.h"
#include "../../entities/player.h"
#include "../app/app_state.h"
#include "../integration/event_bus.h"
#include "../persistence/persistence.h"
#include "../progression/progression_xp.h"
#include <stdio.h>
#include <string.h>

void rogue_player_progress_update(double dt_seconds)
{
    float raw_dt_ms = (float) dt_seconds * 1000.0f;
    /* Level-up loop */
    while (g_app.player.xp >= g_app.player.xp_to_next)
    {
        int old_level = g_app.player.level;
        g_app.player.xp -= g_app.player.xp_to_next;
        g_app.player.level++;
        g_app.unspent_stat_points += 3;
        g_app.talent_points += 1; /* grant a talent point each level */
        /* Persist talent point gain immediately so quitting right after still saves it */
        rogue_persistence_save_player_stats();
        /* Recompute xp_to_next using infinite curve */
        g_app.player.xp_to_next = (int) rogue_xp_to_next_for_level(g_app.player.level);
        rogue_player_recalc_derived(&g_app.player);
        g_app.player.health = g_app.player.max_health;
        g_app.player.mana = g_app.player.max_mana;
        g_app.levelup_aura_timer_ms = 2000.0f;
        /* Route through FX bus using registry id 'LEVELUP' if present */
        RogueEffectEvent ev = {0};
        ev.type = ROGUE_FX_AUDIO_PLAY;
        ev.priority = ROGUE_FX_PRI_UI;
#if defined(_MSC_VER)
        strncpy_s(ev.id, sizeof ev.id, "LEVELUP", _TRUNCATE);
#else
        strncpy(ev.id, "LEVELUP", sizeof ev.id - 1);
        ev.id[sizeof ev.id - 1] = '\0';
#endif
        rogue_fx_emit(&ev);
        g_app.stats_dirty = 1;
        /* Publish LEVEL_UP event */
        RogueEventPayload pl = {0};
        pl.level_up.player_id = 0;
        pl.level_up.old_level = (uint8_t) old_level;
        pl.level_up.new_level = (uint8_t) g_app.player.level;
        rogue_event_publish(ROGUE_EVENT_LEVEL_UP, &pl, ROGUE_EVENT_PRIORITY_HIGH, 0x50524F47,
                            "progression");
        /* Immediate save on each level-up to ensure persistence even if player quits before
         * autosave interval */
        rogue_persistence_save_player_stats();
    }
    /* Accumulate lifetime XP (safe saturating) */
    if (g_app.player.xp > 0)
    { /* current partial progress */
        unsigned long long partial = (unsigned long long) g_app.player.xp;
        if (g_app.player.xp_total_accum < ULLONG_MAX)
        {
            rogue_xp_safe_add(&g_app.player.xp_total_accum, partial);
        }
    }
    g_app.difficulty_scalar =
        1.0 + (double) g_app.player.level * 0.15 + (double) g_app.total_kills * 0.002;
    /* Passive regen */
    g_app.time_since_player_hit_ms += raw_dt_ms;
    if (g_app.player.health > 0 && g_app.player.health < g_app.player.max_health)
    {
        if (g_app.time_since_player_hit_ms > 4000.0f)
        {
            g_app.health_regen_accum_ms += raw_dt_ms;
            float interval = 900.0f - (g_app.player.vitality * 4.0f);
            if (interval < 250.0f)
                interval = 250.0f;
            while (g_app.health_regen_accum_ms >= interval)
            {
                g_app.health_regen_accum_ms -= interval;
                g_app.player.health += 1 + g_app.player.vitality / 25;
                if (g_app.player.health > g_app.player.max_health)
                    g_app.player.health = g_app.player.max_health;
            }
        }
    }
    else
    {
        g_app.health_regen_accum_ms = 0.0f;
    }
    if (g_app.player.mana < g_app.player.max_mana)
    {
        g_app.mana_regen_accum_ms += raw_dt_ms;
        float interval_mp = 520.0f - g_app.player.intelligence * 6.5f;
        if (interval_mp < 120.0f)
            interval_mp = 120.0f;
        if (g_app.time_since_player_hit_ms > 4000.0f)
            interval_mp *= 0.85f;
        while (g_app.mana_regen_accum_ms >= interval_mp)
        {
            g_app.mana_regen_accum_ms -= interval_mp;
            g_app.player.mana += 1 + g_app.player.intelligence / 12;
            if (g_app.player.mana > g_app.player.max_mana)
                g_app.player.mana = g_app.player.max_mana;
        }
    }
    else
    {
        g_app.mana_regen_accum_ms = 0.0f;
    }
    /* Overdrive/Exhaustion timers (Phase 2.4) */
    if (g_app.ap_overdrive_ms > 0.0f)
    {
        g_app.ap_overdrive_ms -= raw_dt_ms;
        if (g_app.ap_overdrive_ms <= 0.0f)
        {
            g_app.ap_overdrive_ms = 0.0f;
            /* End of overdrive -> begin exhaustion if configured (>0) */
            if (g_app.ap_exhaustion_ms < 500.0f)
            {
                /* ensure at least a brief penalty window */
                g_app.ap_exhaustion_ms += 1500.0f;
            }
        }
    }
    if (g_app.ap_exhaustion_ms > 0.0f)
    {
        g_app.ap_exhaustion_ms -= raw_dt_ms;
        if (g_app.ap_exhaustion_ms < 0.0f)
            g_app.ap_exhaustion_ms = 0.0f;
    }
    /* Action Point regen (Phase 1.5 core + soft throttle + exhaustion). */
    if (g_app.player.action_points < g_app.player.max_action_points)
    {
        g_app.ap_regen_accum_ms += raw_dt_ms;
        float interval_ap = 180.0f - g_app.player.dexterity * 1.5f;
        if (interval_ap < 60.0f)
            interval_ap = 60.0f;
        /* Soft throttle: while active, increase interval (slower regen) */
        if (g_app.ap_throttle_timer_ms > 0.0f)
        {
            interval_ap *= 1.8f;
        }
        /* Exhaustion stacks with throttle for stronger slowdown */
        if (g_app.ap_exhaustion_ms > 0.0f)
        {
            interval_ap *= 1.5f;
        }
        while (g_app.ap_regen_accum_ms >= interval_ap)
        {
            g_app.ap_regen_accum_ms -= interval_ap;
            int gain = 2 + g_app.player.dexterity / 15;
            if (gain < 1)
                gain = 1;
            if (g_app.ap_throttle_timer_ms > 0.0f)
            {
                /* Throttled gain reduced */
                if (gain > 1)
                    gain -= 1;
            }
            if (g_app.ap_exhaustion_ms > 0.0f && gain > 0)
            {
                gain -= 1; /* mild additional reduction */
            }
            g_app.player.action_points += gain;
            if (g_app.player.action_points > g_app.player.max_action_points)
                g_app.player.action_points = g_app.player.max_action_points;
        }
    }
    else
    {
        g_app.ap_regen_accum_ms = 0.0f;
    }
    if (g_app.ap_throttle_timer_ms > 0.0f)
    {
        g_app.ap_throttle_timer_ms -= raw_dt_ms;
        if (g_app.ap_throttle_timer_ms < 0.0f)
            g_app.ap_throttle_timer_ms = 0.0f;
    }
    /* Heat venting (Phase 2.5): passive vent when not overheated; when overheated, vent faster */
    if (g_app.player.heat > 0)
    {
        g_app.heat_vent_accum_ms += raw_dt_ms;
        float vent_interval = 320.0f; /* base vent tick every 320ms */
        int vent_amount = 1;
        if (g_app.overheat_active)
        {
            vent_interval = 220.0f;
            vent_amount = 2; /* faster vent while overheated */
        }
        while (g_app.heat_vent_accum_ms >= vent_interval)
        {
            g_app.heat_vent_accum_ms -= vent_interval;
            g_app.player.heat -= vent_amount;
            if (g_app.player.heat <= 0)
            {
                g_app.player.heat = 0;
                g_app.overheat_active = 0; /* clear overheat once cooled to 0 */
                break;
            }
        }
    }
    else
    {
        g_app.heat_vent_accum_ms = 0.0f;
    }
    /* Autosave moved to persistence_autosave module */
}

void rogue_overdrive_begin(int ap_bonus, float duration_ms, float exhaustion_ms)
{
    if (ap_bonus <= 0 || duration_ms <= 0.0f)
        return;
    g_app.ap_overdrive_bonus = ap_bonus;
    g_app.ap_overdrive_ms = duration_ms;
    /* exhaustion applied at end automatically; store as pending amount */
    if (exhaustion_ms > 0.0f)
        g_app.ap_exhaustion_ms = exhaustion_ms;
    /* On begin, allow current AP to fill up to the new cap via regen/spend checks. */
}

void rogue_heat_add(int amount)
{
    if (amount <= 0)
        return;
    g_app.player.heat += amount;
    if (g_app.player.heat >= g_app.player.max_heat)
    {
        g_app.player.heat = g_app.player.max_heat;
        g_app.overheat_active = 1;
    }
}
