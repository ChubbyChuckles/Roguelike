#include "effect_spec.h"
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
    unsigned int seq; /* tie-breaker for deterministic ordering when when_ms equal */
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

void rogue_effect_apply(int id, double now_ms)
{
    (void) now_ms; /* now_ms kept for future time-based scaling */
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
    switch (s->kind)
    {
    case ROGUE_EFFECT_STAT_BUFF:
    {
        /* Default to ADD stacking if rule not specified in spec (tests expect additive). */
        RogueBuffStackRule rule = (RogueBuffStackRule) s->stack_rule;
        if (rule < ROGUE_BUFF_STACK_UNIQUE || rule > ROGUE_BUFF_STACK_REPLACE_IF_STRONGER)
            rule = ROGUE_BUFF_STACK_ADD;
        /* If spec left rule at zero (UNIQUE), treat as default ADD for EffectSpec semantics
           unless author explicitly set another valid rule. */
        if (s->stack_rule == 0)
            rule = ROGUE_BUFF_STACK_ADD;
        rogue_buffs_apply((RogueBuffType) s->buff_type, s->magnitude, s->duration_ms, now_ms, rule,
                          s->snapshot ? 1 : 0);
    }
        if (s->pulse_period_ms > 0.0f && s->duration_ms > 0.0f)
        {
            /* schedule subsequent pulses within duration window */
            double t = now_ms + (double) s->pulse_period_ms;
            double end = now_ms + (double) s->duration_ms;
            while (t <= end && g_event_count < ROGUE_EFFECT_EV_CAP)
            {
                push_event(id, t);
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
        break;
    default: /* future kinds */
        break;
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
        rogue_effect_apply(ev.effect_id, now_ms);
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
