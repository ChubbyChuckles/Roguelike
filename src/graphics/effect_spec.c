/**
 * @file effect_spec.c
 * @brief Implementation of effect specification system for managing game effects like buffs, DOTs,
 * and auras.
 *
 * This module handles the registration, application, and management of various game effects
 * including:
 * - STAT_BUFF effects for temporary stat modifications
 * - DOT (Damage Over Time) effects for periodic damage
 * - AURA effects for area-of-effect abilities
 * - Child effect chaining with delays
 * - AURA exclusivity groups
 * - Dynamic magnitude scaling based on buff types
 * - Precondition gates based on buff requirements
 */

#include "effect_spec.h"
#include "../core/app/app_state.h"
#include "../game/buffs.h"
#include <stdlib.h>
#include <string.h>

static RogueEffectSpec* g_effect_specs = NULL;
static int g_effect_spec_count = 0;
static int g_effect_spec_cap = 0;

/* Phase 3.5: minimal pending events queue for periodic pulses & child chains */
typedef struct RogueEffectEvent
{
    int effect_id;
    double when_ms;
    unsigned int seq;       /* tie-breaker for deterministic ordering when when_ms equal */
    int override_magnitude; /* >=0 to force magnitude (snapshot_scale pulses) */
    signed char
        force_crit; /* -1 = unset (compute), 0 = no crit, 1 = crit (per-application snapshot) */
} RogueEffectEvent;

#define ROGUE_EFFECT_EV_CAP 256
static RogueEffectEvent g_events[ROGUE_EFFECT_EV_CAP];
static int g_event_count = 0;

static unsigned int g_event_seq = 0;

/* ---- Phase 6.3: Minimal active AURA tracking for exclusivity groups ---- */
typedef struct ActiveAuraRec
{
    int effect_id;
    double end_ms;           /* when this AURA instance should end */
    double last_apply_ms;    /* when most recently (re)applied */
    unsigned int group_mask; /* exclusivity group mask */
    int magnitude_snapshot;  /* last computed magnitude for compare */
} ActiveAuraRec;

#define ROGUE_ACTIVE_AURA_CAP 64
static ActiveAuraRec g_active_auras[ROGUE_ACTIVE_AURA_CAP];
static int g_active_aura_count = 0;

/**
 * @brief Finds the index of an active aura by effect ID.
 * @param effect_id The effect ID to search for.
 * @return The index of the active aura, or -1 if not found.
 */
static int find_active_aura_index(int effect_id)
{
    for (int i = 0; i < g_active_aura_count; ++i)
    {
        if (g_active_auras[i].effect_id == effect_id)
            return i;
    }
    return -1;
}

/**
 * @brief Removes an active aura at the specified index.
 * @param idx The index of the active aura to remove.
 */
static void remove_active_aura_at(int idx)
{
    if (idx < 0 || idx >= g_active_aura_count)
        return;
    g_active_auras[idx] = g_active_auras[g_active_aura_count - 1];
    g_active_aura_count--;
}

/* ---- Phase 11.3: minimal getters for active aura overlay ---- */
/**
 * @brief Gets the count of currently active auras.
 * @return The number of active auras.
 */
int rogue_effect_active_aura_count(void) { return g_active_aura_count; }

/**
 * @brief Gets information about an active aura at the specified index.
 * @param index The index of the active aura to query.
 * @param effect_id Pointer to store the effect ID (can be NULL).
 * @param end_ms Pointer to store the end time in milliseconds (can be NULL).
 * @return 1 if the index is valid and data was retrieved, 0 otherwise.
 */
int rogue_effect_active_aura_get(int index, int* effect_id, double* end_ms)
{
    if (index < 0 || index >= g_active_aura_count)
        return 0;
    if (effect_id)
        *effect_id = g_active_auras[index].effect_id;
    if (end_ms)
        *end_ms = g_active_auras[index].end_ms;
    return 1;
}

/**
 * @brief Finds any active aura that conflicts with the provided group mask and is not expired.
 * @param group_mask The group mask to check for conflicts.
 * @param now_ms The current time in milliseconds.
 * @return The index of the conflicting aura, or -1 if no conflict found.
 */
static int find_conflicting_aura_index(unsigned int group_mask, double now_ms)
{
    if (!group_mask)
        return -1;
    for (int i = 0; i < g_active_aura_count; ++i)
    {
        if ((g_active_auras[i].group_mask & group_mask) != 0u && g_active_auras[i].end_ms > now_ms)
            return i;
    }
    return -1;
}

/* ---- Phase 6.2: Spatial query hook (fallback O(N)) ---- */
/**
 * @brief Collects enemy indices within a specified radius of a center point.
 * @param cx The x-coordinate of the center point.
 * @param cy The y-coordinate of the center point.
 * @param radius The radius to search within.
 * @param out_indices Array to store the collected enemy indices.
 * @param cap The maximum number of indices to collect.
 * @return The number of enemies found within the radius.
 */
static int rogue_collect_enemies_in_radius(float cx, float cy, float radius, int* out_indices,
                                           int cap)
{
    if (radius <= 0.0f || !out_indices || cap <= 0)
        return 0;
    float r2 = radius * radius;
    int n = 0;
    for (int i = 0; i < g_app.enemy_count && n < cap; ++i)
    {
        RogueEnemy* e = &g_app.enemies[i];
        if (!e->alive)
            continue;
        float dx = e->base.pos.x - cx;
        float dy = e->base.pos.y - cy;
        float d2 = dx * dx + dy * dy;
        if (d2 <= r2)
            out_indices[n++] = i;
    }
    return n;
}

/* Phase 5.3/5.5: Minimal active DOT tracking per effect id to support stacking models
   (UNIQUE, REFRESH, EXTEND). This is intentionally simple (single target assumption in tests).
*/
typedef struct ActiveDOTRec
{
    int effect_id;
    double end_ms;        /* when this DOT instance should end */
    double last_apply_ms; /* when the DOT was most recently (re)applied */
} ActiveDOTRec;

#define ROGUE_ACTIVE_DOT_CAP 64
static ActiveDOTRec g_active_dots[ROGUE_ACTIVE_DOT_CAP];
static int g_active_dot_count = 0;

/**
 * @brief Finds the index of an active DOT by effect ID.
 * @param effect_id The effect ID to search for.
 * @return The index of the active DOT, or -1 if not found.
 */
static int find_active_dot_index(int effect_id)
{
    for (int i = 0; i < g_active_dot_count; ++i)
    {
        if (g_active_dots[i].effect_id == effect_id)
            return i;
    }
    return -1;
}

/**
 * @brief Removes all pending events for a specific effect ID.
 * @param effect_id The effect ID to remove pending events for.
 */
static void remove_pending_for_effect(int effect_id)
{
    for (int i = 0; i < g_event_count;)
    {
        if (g_events[i].effect_id == effect_id)
        {
            g_events[i] = g_events[g_event_count - 1];
            g_event_count--;
            continue;
        }
        ++i;
    }
}

/**
 * @brief Pushes a new effect event to the event queue.
 * @param effect_id The effect ID for the event.
 * @param when_ms The time in milliseconds when the event should occur.
 */
static void push_event(int effect_id, double when_ms)
{
    if (g_event_count >= ROGUE_EFFECT_EV_CAP)
        return;
    g_events[g_event_count].effect_id = effect_id;
    g_events[g_event_count].when_ms = when_ms;
    g_events[g_event_count].seq = g_event_seq++;
    g_events[g_event_count].override_magnitude = -1;
    g_events[g_event_count].force_crit = -1;
    g_event_count++;
}

/**
 * @brief Resets the effect system, clearing all registered effects and active states.
 */
void rogue_effect_reset(void)
{
    free(g_effect_specs);
    g_effect_specs = NULL;
    g_effect_spec_count = 0;
    g_effect_spec_cap = 0;
    g_event_count = 0;
    g_event_seq = 0;
    g_active_dot_count = 0;
    g_active_aura_count = 0;
}

/**
 * @brief Registers a new effect specification.
 * @param spec The effect specification to register.
 * @return The ID of the registered effect, or -1 on failure.
 */
int rogue_effect_register(const RogueEffectSpec* spec)
{
    if (!spec)
        return -1;
    if (g_effect_spec_count == g_effect_spec_cap)
    {
        int nc = g_effect_spec_cap ? g_effect_spec_cap * 2 : 8;
        RogueEffectSpec* ns = (RogueEffectSpec*) realloc(g_effect_specs, (size_t) nc * sizeof *ns);
        if (!ns)
            return -1;
        g_effect_specs = ns;
        g_effect_spec_cap = nc;
    }
    /* Copy spec then apply safe defaults only when fields appear unset (zeroed). */
    RogueEffectSpec tmp = *spec;
    /* If author didn't specify a precondition, treat as none (0xFFFF sentinel). */
    if (tmp.require_buff_type == 0)
        tmp.require_buff_type = (unsigned short) 0xFFFFu;
    /* Default scale_by to none if unset. */
    if (tmp.scale_by_buff_type == 0)
        tmp.scale_by_buff_type = (unsigned short) 0xFFFFu;
    if (tmp.kind == ROGUE_EFFECT_DOT)
    {
        if (!tmp.debuff)
            tmp.debuff = 1;
    }
    else if (tmp.kind == ROGUE_EFFECT_AURA)
    {
        /* AURA defaults: debuff=1 if magnitude implies damage; radius default 1.5 tiles */
        if (!tmp.debuff && tmp.magnitude > 0)
            tmp.debuff = 1;
        if (tmp.aura_radius <= 0.0f)
            tmp.aura_radius = 1.5f;
    }
    /* Default stacking behavior: for STAT_BUFF effects, default to ADD when unspecified.
       Tests construct specs with zero-initialized fields; a zero stack_rule maps to UNIQUE in
       the enum, but the intended default for buffs is additive stacking. Config parser already
       sets ADD explicitly when not provided; mirror that here for programmatic specs. */
    if (tmp.kind == ROGUE_EFFECT_STAT_BUFF && tmp.stack_rule == 0)
    {
        tmp.stack_rule = (unsigned char) ROGUE_BUFF_STACK_ADD;
    }
    tmp.id = g_effect_spec_count;
    g_effect_specs[g_effect_spec_count] = tmp;
    return g_effect_spec_count++;
}

/**
 * @brief Gets the effect specification for a given effect ID.
 * @param id The effect ID to retrieve.
 * @return Pointer to the effect specification, or NULL if not found.
 */
const RogueEffectSpec* rogue_effect_get(int id)
{
    if (id < 0 || id >= g_effect_spec_count)
        return NULL;
    return &g_effect_specs[id];
}

/**
 * @brief Computes the effective magnitude of an effect with optional scaling.
 * @param s The effect specification.
 * @return The computed effective magnitude.
 */
static int compute_scaled_magnitude(const RogueEffectSpec* s)
{
    int mag = s->magnitude;
    if (s->scale_by_buff_type != (unsigned short) 0xFFFFu && s->scale_pct_per_point != 0)
    {
        int total = rogue_buffs_get_total((RogueBuffType) s->scale_by_buff_type);
        long long pct = 100 + (long long) s->scale_pct_per_point * (long long) total;
        if (pct < 0)
            pct = 0; /* clamp */
        long long nm = ((long long) mag * pct) / 100;
        if (nm > 999)
            nm = 999;
        if (nm < 0)
            nm = 0;
        mag = (int) nm;
    }
    return mag;
}

/* One-shot crit override channel for the next apply_with_magnitude() invocation.
   Values: -2 (unset), 0 (force non-crit), 1 (force crit). */
static int g_effects_force_next_crit = -2;

/* Simple deterministic hash -> [0,99] used for RNG-less crit decisions. */
static unsigned int hash_to_pct(unsigned int a, unsigned int b, unsigned int c)
{
    /* xorshift mix */
    unsigned int x = a * 1664525u + 1013904223u;
    x ^= (b + 0x9E3779B9u) + (x << 6) + (x >> 2);
    x ^= (c + 0x85EBCA6Bu) + (x << 13) + (x >> 7);
    return x % 100u;
}

static void apply_with_magnitude(const RogueEffectSpec* s, int eff_mag, double now_ms)
{
    switch (s->kind)
    {
    case ROGUE_EFFECT_STAT_BUFF:
    {
        RogueBuffStackRule rule = (RogueBuffStackRule) s->stack_rule;
        if (rule < ROGUE_BUFF_STACK_UNIQUE || rule > ROGUE_BUFF_STACK_REPLACE_IF_STRONGER)
            rule = ROGUE_BUFF_STACK_ADD;
        rogue_buffs_apply((RogueBuffType) s->buff_type, eff_mag, s->duration_ms, now_ms, rule,
                          s->snapshot ? 1 : 0);
    }
    break;
    case ROGUE_EFFECT_DOT:
    {
        /* Choose first alive enemy as target for deterministic unit tests */
        RogueEnemy* target = NULL;
        for (int i = 0; i < g_app.enemy_count; ++i)
        {
            if (g_app.enemies[i].alive)
            {
                target = &g_app.enemies[i];
                break;
            }
        }
        if (!target)
            break;
        /* (debug prints removed) */
        int over = 0;
        unsigned char dmg_type = s->damage_type;
        int raw = (eff_mag > 0 ? eff_mag : 0);
        /* Crit decision: prefer one-shot override, then global test hook, then deterministic RNG */
        extern int g_force_crit_mode; /* declared in combat.h */
        unsigned char crit = 0;
        if (g_effects_force_next_crit >= 0)
        {
            crit = (unsigned char) (g_effects_force_next_crit ? 1 : 0);
        }
        else if (g_force_crit_mode >= 0)
        {
            crit = (unsigned char) (g_force_crit_mode ? 1 : 0);
        }
        else if (s->crit_chance_pct > 0)
        {
            /* Use a stable hash of (effect id, current sequence watermark, time bucket) */
            unsigned int pct =
                hash_to_pct((unsigned int) s->id, g_event_seq, (unsigned int) now_ms);
            crit = (unsigned char) (pct < (unsigned int) s->crit_chance_pct ? 1 : 0);
        }
        if (crit)
        {
            long long nm = (long long) raw * 150LL; /* 150% crit */
            raw = (int) (nm / 100LL);
        }
        int mitig = rogue_apply_mitigation_enemy(target, raw, dmg_type, &over);
        if (mitig < 0)
            mitig = 0;
        if (target->health > 0)
        {
            int nh = target->health - mitig;
            target->health = (nh < 0 ? 0 : nh);
            if (target->health == 0)
                target->alive = 0;
        }
        /* (debug prints removed) */
        rogue_damage_event_record(0, dmg_type, crit, raw, mitig, over, 0);
    }
    break;
    case ROGUE_EFFECT_AURA:
    {
        /* Area effect: apply to all enemies within radius of player (via spatial hook) */
        float px = g_app.player.base.pos.x;
        float py = g_app.player.base.pos.y;
        float r = (s->aura_radius > 0.0f) ? s->aura_radius : 0.0f;
        unsigned char dmg_type = s->damage_type; /* allow damage typing for auras */
        int base_raw = (eff_mag > 0 ? eff_mag : 0);
        int idxs[128];
        int nidx =
            rogue_collect_enemies_in_radius(px, py, r, idxs, (int) (sizeof idxs / sizeof idxs[0]));
        for (int ii = 0; ii < nidx; ++ii)
        {
            RogueEnemy* e = &g_app.enemies[idxs[ii]];
            int over = 0;
            int raw = base_raw;
            /* Optional per-tick crit akin to DOT per-tick (deterministic hash) */
            unsigned char crit = 0;
            extern int g_force_crit_mode;
            if (g_effects_force_next_crit >= 0)
                crit = (unsigned char) (g_effects_force_next_crit ? 1 : 0);
            else if (g_force_crit_mode >= 0)
                crit = (unsigned char) (g_force_crit_mode ? 1 : 0);
            else if (s->crit_chance_pct > 0)
            {
                unsigned int pct = hash_to_pct((unsigned int) s->id, (unsigned int) idxs[ii],
                                               (unsigned int) now_ms);
                crit = (unsigned char) (pct < (unsigned int) s->crit_chance_pct ? 1 : 0);
            }
            if (crit)
            {
                long long nm = (long long) raw * 150LL;
                raw = (int) (nm / 100LL);
            }
            int mitig = rogue_apply_mitigation_enemy(e, raw, dmg_type, &over);
            if (mitig < 0)
                mitig = 0;
            if (e->health > 0)
            {
                int nh = e->health - mitig;
                e->health = (nh < 0 ? 0 : nh);
                if (e->health == 0)
                    e->alive = 0;
            }
            rogue_damage_event_record(0, dmg_type, crit, raw, mitig, over, 0);
        }
    }
    break;
    default:
        break;
    }
}

/**
 * @brief Applies an effect by ID at the specified time.
 *
 * This function handles the complete application of an effect including:
 * - Precondition checking based on required buffs
 * - Magnitude scaling based on buff types
 * - AURA exclusivity group management
 * - DOT stacking semantics (UNIQUE, REFRESH, EXTEND)
 * - Event scheduling for periodic effects
 * - Child effect chaining with delays
 *
 * @param id The effect ID to apply.
 * @param now_ms The current time in milliseconds.
 */
void rogue_effect_apply(int id, double now_ms)
{
    const RogueEffectSpec* s = rogue_effect_get(id);
    if (!s)
        return;
    /* Phase 3.2: simple precondition gate. If require_buff_type set, ensure present. */
    if (s->require_buff_type != (unsigned short) 0xFFFFu)
    {
        int have = rogue_buffs_get_total((RogueBuffType) s->require_buff_type);
        int need = (s->require_buff_min > 0) ? s->require_buff_min : 1;
        if (have < need)
            return; /* gate blocked */
    }
    /* Phase 3.4: compute scaled magnitude (dynamic) for initial application. */
    int eff_mag = compute_scaled_magnitude(s);
    /* Phase 6.3: AURA exclusivity groups (replace-if-stronger). Do this before any immediate apply.
     */
    if (s->kind == ROGUE_EFFECT_AURA && s->aura_group_mask != 0u)
    {
        int cidx = find_conflicting_aura_index(s->aura_group_mask, now_ms);
        if (cidx >= 0)
        {
            /* If existing is stronger or equal and still active, ignore this apply. */
            if (g_active_auras[cidx].magnitude_snapshot >= eff_mag)
            {
                return;
            }
            /* Otherwise, replace: cancel pending of the weaker one and drop its active record. */
            remove_pending_for_effect(g_active_auras[cidx].effect_id);
            remove_active_aura_at(cidx);
        }
    }
    /* Phase 5.4: per-application crit snapshot option */
    int snapshot_cd = -2;
    if (s->kind == ROGUE_EFFECT_DOT && s->crit_mode == 1)
    {
        extern int g_force_crit_mode;
        if (g_force_crit_mode >= 0)
            snapshot_cd = (g_force_crit_mode ? 1 : 0);
        else if (s->crit_chance_pct > 0)
            snapshot_cd = (hash_to_pct((unsigned int) s->id, g_event_seq, (unsigned int) now_ms) <
                           (unsigned int) s->crit_chance_pct)
                              ? 1
                              : 0;
    }
    int prev_force = g_effects_force_next_crit;
    if (snapshot_cd >= 0)
        g_effects_force_next_crit = snapshot_cd;
    /* Phase 5.3: DOT stacking semantics for UNIQUE/REFRESH/EXTEND */
    double schedule_end_ms = now_ms + (double) s->duration_ms;
    int stacking_rule = (s->stack_rule <= ROGUE_BUFF_STACK_REPLACE_IF_STRONGER)
                            ? s->stack_rule
                            : ROGUE_BUFF_STACK_ADD;
    if (s->kind == ROGUE_EFFECT_DOT && s->duration_ms > 0.0f)
    {
        int idx = find_active_dot_index(id);
        int is_active = (idx >= 0 && g_active_dots[idx].end_ms > now_ms);
        if (stacking_rule == ROGUE_BUFF_STACK_UNIQUE && is_active)
        {
            /* Do not apply new instance */
            return;
        }
        else if ((stacking_rule == ROGUE_BUFF_STACK_REFRESH ||
                  stacking_rule == ROGUE_BUFF_STACK_EXTEND) &&
                 is_active)
        {
            /* Recompute end; for REFRESH we also remove future pulses to realign. */
            double remaining = g_active_dots[idx].end_ms - now_ms;
            if (remaining < 0.0)
                remaining = 0.0;
            double new_total = (stacking_rule == ROGUE_BUFF_STACK_EXTEND)
                                   ? (remaining + (double) s->duration_ms)
                                   : (double) s->duration_ms;
            schedule_end_ms = now_ms + new_total;
            if (stacking_rule == ROGUE_BUFF_STACK_REFRESH)
            {
                /* For REFRESH semantics, cancel previously scheduled pulses. */
                remove_pending_for_effect(id);
            }
            g_active_dots[idx].end_ms = schedule_end_ms;
            g_active_dots[idx].last_apply_ms = now_ms;
        }
        else
        {
            /* New active record (or additive case - track last one) */
            if (idx < 0)
            {
                if (g_active_dot_count < ROGUE_ACTIVE_DOT_CAP)
                {
                    g_active_dots[g_active_dot_count].effect_id = id;
                    g_active_dots[g_active_dot_count].end_ms = schedule_end_ms;
                    g_active_dots[g_active_dot_count].last_apply_ms = now_ms;
                    g_active_dot_count++;
                }
            }
            else
            {
                /* Update the tracked end to the later one */
                if (g_active_dots[idx].end_ms < schedule_end_ms)
                    g_active_dots[idx].end_ms = schedule_end_ms;
                /* Update last apply time to now; used for REFRESH filtering of stale pulses. */
                g_active_dots[idx].last_apply_ms = now_ms;
            }
        }
    }

    apply_with_magnitude(s, eff_mag, now_ms);
    g_effects_force_next_crit = prev_force;
    if (s->pulse_period_ms > 0.0f && s->duration_ms > 0.0f)
    {
        /* schedule subsequent pulses within duration (or updated schedule_end_ms) */
        double t = now_ms + (double) s->pulse_period_ms;
        double end = schedule_end_ms;
        while (t <= end && g_event_count < ROGUE_EFFECT_EV_CAP)
        {
            push_event(id, t);
            if (s->snapshot_scale)
                g_events[g_event_count - 1].override_magnitude = eff_mag;
            /* Carry per-application crit snapshot to all pulses */
            if (snapshot_cd >= 0)
                g_events[g_event_count - 1].force_crit = (signed char) snapshot_cd;
            t += (double) s->pulse_period_ms;
        }
    }
    /* Phase 6.3: Track active AURAs for exclusivity and refresh behavior. */
    if (s->kind == ROGUE_EFFECT_AURA && s->duration_ms > 0.0f)
    {
        int idx = find_active_aura_index(id);
        if (idx < 0)
        {
            if (g_active_aura_count < ROGUE_ACTIVE_AURA_CAP)
            {
                g_active_auras[g_active_aura_count].effect_id = id;
                g_active_auras[g_active_aura_count].end_ms = schedule_end_ms;
                g_active_auras[g_active_aura_count].last_apply_ms = now_ms;
                g_active_auras[g_active_aura_count].group_mask = s->aura_group_mask;
                g_active_auras[g_active_aura_count].magnitude_snapshot = eff_mag;
                g_active_aura_count++;
            }
        }
        else
        {
            if (g_active_auras[idx].end_ms < schedule_end_ms)
                g_active_auras[idx].end_ms = schedule_end_ms;
            g_active_auras[idx].last_apply_ms = now_ms;
            if (g_active_auras[idx].magnitude_snapshot < eff_mag)
                g_active_auras[idx].magnitude_snapshot = eff_mag;
            g_active_auras[idx].group_mask = s->aura_group_mask;
        }
    }
    /* schedule children */
    for (int i = 0; i < (int) s->child_count && i < 4; ++i)
    {
        const RogueEffectChild* ch = &s->children[i];
        if (ch->child_effect_id >= 0)
            push_event(ch->child_effect_id, now_ms + (double) ch->delay_ms);
    }
}

/**
 * @brief Updates the effect system, processing all ready events.
 *
 * This function processes pending effect events in chronological order, handling:
 * - Periodic DOT pulses with proper timing
 * - Child effect activation with delays
 * - Stale event filtering for REFRESH stacking
 * - Magnitude scaling and crit overrides for scheduled events
 * - Deterministic event ordering using sequence numbers
 *
 * @param now_ms The current time in milliseconds.
 */
void rogue_effects_update(double now_ms)
{
    /* Process ready events in stable order (when_ms asc, then seq asc). */
    for (;;)
    {
        int picked = -1;
        double best_t = 0.0;
        unsigned int best_seq = 0;
        for (int i = 0; i < g_event_count; ++i)
        {
            RogueEffectEvent ev = g_events[i];
            if (ev.when_ms <= now_ms)
            {
                if (picked < 0 || ev.when_ms < best_t ||
                    (ev.when_ms == best_t && ev.seq < best_seq))
                {
                    picked = i;
                    best_t = ev.when_ms;
                    best_seq = ev.seq;
                }
            }
        }
        if (picked < 0)
            break;
        RogueEffectEvent ev = g_events[picked];
        /* remove picked by swapping with last and reducing count */
        g_events[picked] = g_events[g_event_count - 1];
        g_event_count--;
        const RogueEffectSpec* s = rogue_effect_get(ev.effect_id);
        if (!s)
            continue;
        /* If this is a DOT with REFRESH stacking, ignore pulses that were scheduled prior to
           the last refresh boundary (defensive against any stale events). */
        if (s->kind == ROGUE_EFFECT_DOT && s->stack_rule == ROGUE_BUFF_STACK_REFRESH)
        {
            int didx = find_active_dot_index(ev.effect_id);
            if (didx >= 0)
            {
                double boundary = g_active_dots[didx].last_apply_ms + (double) s->pulse_period_ms;
                if (ev.when_ms < boundary)
                {
                    /* Skip this stale pulse */
                    continue;
                }
            }
        }
        int mag =
            (ev.override_magnitude >= 0) ? ev.override_magnitude : compute_scaled_magnitude(s);
        int prev_force = g_effects_force_next_crit;
        if (ev.force_crit >= 0)
            g_effects_force_next_crit = ev.force_crit;
        apply_with_magnitude(s, mag, ev.when_ms);
        g_effects_force_next_crit = prev_force;
    }
}

/**
 * @brief Checks if an effect is considered a debuff.
 *
 * An effect is considered a debuff if:
 * - The debuff flag is explicitly set in the spec
 * - The effect is a DOT (Damage Over Time) type
 *
 * @param id The effect ID to check.
 * @return 1 if the effect is a debuff, 0 otherwise.
 */
int rogue_effect_spec_is_debuff(int id)
{
    const RogueEffectSpec* s = rogue_effect_get(id);
    if (!s)
        return 0;
    if (s->debuff)
        return 1;
    return (s->kind == ROGUE_EFFECT_DOT) ? 1 : 0;
}
