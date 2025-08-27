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

void rogue_effect_reset(void)
{
    free(g_effect_specs);
    g_effect_specs = NULL;
    g_effect_spec_count = 0;
    g_effect_spec_cap = 0;
    g_event_count = 0;
    g_event_seq = 0;
}

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
    tmp.id = g_effect_spec_count;
    g_effect_specs[g_effect_spec_count] = tmp;
    return g_effect_spec_count++;
}

const RogueEffectSpec* rogue_effect_get(int id)
{
    if (id < 0 || id >= g_effect_spec_count)
        return NULL;
    return &g_effect_specs[id];
}

/* Compute effective magnitude with optional scaling params. */
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
        if (s->stack_rule == 0)
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
        rogue_damage_event_record(0, dmg_type, crit, raw, mitig, over, 0);
    }
    break;
    default:
        break;
    }
}

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
    apply_with_magnitude(s, eff_mag, now_ms);
    g_effects_force_next_crit = prev_force;
    if (s->pulse_period_ms > 0.0f && s->duration_ms > 0.0f)
    {
        /* schedule subsequent pulses within duration window */
        double t = now_ms + (double) s->pulse_period_ms;
        double end = now_ms + (double) s->duration_ms;
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
    /* schedule children */
    for (int i = 0; i < (int) s->child_count && i < 4; ++i)
    {
        const RogueEffectChild* ch = &s->children[i];
        if (ch->child_effect_id >= 0)
            push_event(ch->child_effect_id, now_ms + (double) ch->delay_ms);
    }
}

void rogue_effects_update(double now_ms)
{
    /* Stable order: process ready events in ascending (when_ms, seq). Compact remaining. */
    int w = 0;
    /* Multiple passes could be avoided with a small sort, but we stay O(N^2) bounded by cap. */
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
        if (s)
        {
            int mag =
                (ev.override_magnitude >= 0) ? ev.override_magnitude : compute_scaled_magnitude(s);
            /* Per-tick crit decision if needed (DOT crit_mode==per-tick) */
            int prev_force = g_effects_force_next_crit;
            if (s->kind == ROGUE_EFFECT_DOT)
            {
                int cd = -2;
                extern int g_force_crit_mode;
                if (g_force_crit_mode >= 0)
                    cd = (g_force_crit_mode ? 1 : 0);
                else if (ev.force_crit >= 0)
                    cd = ev.force_crit;
                else if (s->crit_chance_pct > 0)
                {
                    /* Per-tick: use event seq and timestamp for deterministic roll */
                    unsigned int pct =
                        hash_to_pct((unsigned int) s->id, ev.seq, (unsigned int) ev.when_ms);
                    cd = (pct < (unsigned int) s->crit_chance_pct) ? 1 : 0;
                }
                if (cd >= 0)
                    g_effects_force_next_crit = cd;
            }
            apply_with_magnitude(s, mag, now_ms);
            g_effects_force_next_crit = prev_force;
        }
    }
    /* compact non-ready events to front (preserving relative order) */
    for (int i = 0; i < g_event_count; ++i)
    {
        RogueEffectEvent ev = g_events[i];
        if (ev.when_ms > now_ms)
            g_events[w++] = ev;
    }
    g_event_count = w;
}

int rogue_effect_spec_is_debuff(int id)
{
    const RogueEffectSpec* s = rogue_effect_get(id);
    if (!s)
        return 0;
    if (s->debuff)
        return 1;
    return (s->kind == ROGUE_EFFECT_DOT) ? 1 : 0;
}
