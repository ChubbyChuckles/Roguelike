#include "skills_procs.h"
#include "../../graphics/effect_spec.h"
#include "../../util/log.h"
#include "../integration/event_bus.h"
#include <string.h>

#define ROGUE_MAX_PROCS 128
/* Loop guard configuration (Phase 7.4) */
#define ROGUE_PROC_LOOP_DEPTH_MAX 8
#define ROGUE_PROC_CYCLE_SEEN_MAX 32

typedef struct RogueProcRuntime
{
    RogueProcDef def;
    uint32_t sub_id;
    double last_global_ms; /* last trigger time for global ICD */
    /* simple per-target ring buffer */
    struct
    {
        uint32_t target;
        double last_ms;
    } targets[16];
    int target_count;
    /* Phase 7.3 smoothing accumulator (0..100 scale) */
    int acc_pct;
    int in_use;
    /* Phase 7.5 dynamic scaling window (recent activity) */
    double recent_ms[8];
    int recent_count;
} RogueProcRuntime;

static RogueProcRuntime g_procs[ROGUE_MAX_PROCS];
static int g_proc_count = 0;
/* Loop guard state (depth counter + cycle signature seen set) */
static int g_loop_depth = 0;
static unsigned long long g_cycle_seen[ROGUE_PROC_CYCLE_SEEN_MAX];
static int g_cycle_seen_count = 0;

/* Deterministic RNG: cheap integer LCG mixed with event timestamp and a per-proc salt. */
static unsigned int mix_u32(unsigned int a, unsigned int b)
{
    a ^= b + 0x9e3779b9u + (a << 6) + (a >> 2);
    return a;
}
static unsigned long long mix_u64(unsigned long long a, unsigned long long b)
{
    a ^= b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2);
    return a;
}
static int roll_chance_pct(int base_pct, int acc_pct, unsigned int salt)
{
    /* Clamp inputs */
    if (base_pct < 0)
        base_pct = 0;
    if (base_pct > 100)
        base_pct = 100;
    if (acc_pct < 0)
        acc_pct = 0;
    if (acc_pct > 100)
        acc_pct = 100;
    int effective = base_pct + acc_pct;
    if (effective > 100)
        effective = 100;
    /* Deterministic 0..99 */
    unsigned int r = mix_u32(salt, 0xA5A5A5A5u);
    int roll = (int) (r % 100u);
    return roll < effective;
}

static bool proc_event_cb(const RogueEvent* ev, void* user)
{
    (void) user;
    if (!ev)
        return false;
    /* Loop guard depth (Phase 7.4) */
    if (g_loop_depth == 0)
    {
        /* reset seen set at the start of a new top-level callback chain */
        g_cycle_seen_count = 0;
    }
    g_loop_depth++;
    if (g_loop_depth > ROGUE_PROC_LOOP_DEPTH_MAX)
    {
        ROGUE_LOG_WARN("[skills_procs] loop depth %d exceeds max %d; skipping event %u",
                       g_loop_depth, ROGUE_PROC_LOOP_DEPTH_MAX, (unsigned) ev->type_id);
        g_loop_depth--;
        return false; /* drop to break potential cycles */
    }
    /* TEMP DEBUG: trace callback entry */
    fprintf(stderr, "[skills_procs] cb type=%u ts=%llu pri=%d\n", (unsigned) ev->type_id,
            (unsigned long long) ev->timestamp_us, (int) ev->priority);
    /* Find matching procs by iterating (small count) */
    double now_ms = (double) (ev->timestamp_us / 1000.0);
    /* Base signature for cycle detection derives from event type and time bucket */
    unsigned long long base_sig = mix_u64((unsigned long long) ev->type_id,
                                          (unsigned long long) (ev->timestamp_us / 1000ull));
    for (int i = 0; i < g_proc_count; ++i)
    {
        RogueProcRuntime* pr = &g_procs[i];
        if (!pr->in_use || pr->def.event_type != ev->type_id)
            continue;
        fprintf(
            stderr,
            "[skills_procs] consider proc idx=%d evt=%u icdG=%.1f icdT=%.1f chance=%d smooth=%d\n",
            i, (unsigned) ev->type_id, pr->def.icd_global_ms, pr->def.icd_per_target_ms,
            pr->def.chance_pct, pr->def.use_smoothing);
        if (pr->def.predicate && !pr->def.predicate(ev))
            continue;
        /* Global ICD */
        if (pr->def.icd_global_ms > 0.0 && (now_ms - pr->last_global_ms) < pr->def.icd_global_ms)
        {
            fprintf(stderr, "[skills_procs] global ICD blocks (dt=%.3f < %.3f)\n",
                    now_ms - pr->last_global_ms, pr->def.icd_global_ms);
            continue;
        }
        /* Per-target ICD: use damage_event.target_entity_id when available; otherwise 0 */
        uint32_t tgt = 0;
        if (ev->type_id == ROGUE_EVENT_DAMAGE_DEALT || ev->type_id == ROGUE_EVENT_DAMAGE_TAKEN ||
            ev->type_id == ROGUE_EVENT_CRITICAL_HIT)
        {
            tgt = ev->payload.damage_event.target_entity_id;
        }
        else if (ev->type_id == ROGUE_EVENT_SKILL_CHANNEL_TICK)
        {
            /* channel ticks typically affect current target = 0 in tests; leave 0 */
            tgt = 0;
        }
        else if (ev->type_id == ROGUE_EVENT_SKILL_COMBO_SPEND)
        {
            tgt = 0;
        }

        if (pr->def.icd_per_target_ms > 0.0)
        {
            int found = -1;
            for (int k = 0; k < pr->target_count; ++k)
            {
                if (pr->targets[k].target == tgt)
                {
                    found = k;
                    break;
                }
            }
            if (found >= 0)
            {
                if ((now_ms - pr->targets[found].last_ms) < pr->def.icd_per_target_ms)
                {
                    fprintf(stderr,
                            "[skills_procs] per-target ICD blocks tgt=%u (dt=%.3f < %.3f)\n",
                            (unsigned) tgt, now_ms - pr->targets[found].last_ms,
                            pr->def.icd_per_target_ms);
                    continue; /* per-target ICD active */
                }
                pr->targets[found].last_ms = now_ms;
            }
            else if (pr->target_count < (int) (sizeof(pr->targets) / sizeof(pr->targets[0])))
            {
                pr->targets[pr->target_count].target = tgt;
                pr->targets[pr->target_count].last_ms = now_ms;
                pr->target_count++;
                fprintf(stderr, "[skills_procs] per-target track new tgt=%u at %.3f ms\n",
                        (unsigned) tgt, now_ms);
            }
            else
            {
                /* fallback: evict oldest (linear scan) */
                int oldest = 0;
                for (int k = 1; k < pr->target_count; ++k)
                {
                    if (pr->targets[k].last_ms < pr->targets[oldest].last_ms)
                        oldest = k;
                }
                pr->targets[oldest].target = tgt;
                pr->targets[oldest].last_ms = now_ms;
            }
        }

        /* Phase 7.5: dynamic scaling based on recent triggers in a 1000ms window. */
        int recent = 0;
        if (pr->recent_count > 0)
        {
            int w = 0;
            for (int k = 0; k < pr->recent_count; ++k)
            {
                if (now_ms - pr->recent_ms[k] <= 1000.0)
                {
                    pr->recent_ms[w++] = pr->recent_ms[k];
                }
            }
            pr->recent_count = w;
            recent = w;
        }
        int scaled_chance = pr->def.chance_pct;
        if (recent > 1)
        {
            /* Reduce chance by 12% per extra trigger within last second, cap 60% reduction */
            int reduce = (recent - 1) * 12;
            if (reduce > 60)
                reduce = 60;
            scaled_chance -= reduce;
            if (scaled_chance < 0)
                scaled_chance = 0;
        }

        /* Probability weighting (Phase 7.3). If chance_pct < 100, roll.
           Use deterministic salt from event timestamp, type, and proc index. */
        int triggered = 1;
        if (scaled_chance < 100)
        {
            unsigned int salt = (unsigned int) ev->timestamp_us;
            salt = mix_u32(salt, (unsigned int) ev->type_id);
            salt = mix_u32(salt, (unsigned int) i);
            triggered =
                roll_chance_pct(scaled_chance, pr->def.use_smoothing ? pr->acc_pct : 0, salt);
            fprintf(stderr, "[skills_procs] chance roll => %s (acc=%d)\n",
                    triggered ? "HIT" : "MISS", pr->acc_pct);
        }
        if (!triggered)
        {
            /* Missed roll: increase accumulator slightly to smooth variance */
            if (pr->def.use_smoothing)
            {
                int add = (100 - pr->def.chance_pct) / 4; /* quarter of gap */
                if (add < 1)
                    add = 1;
                pr->acc_pct += add;
                if (pr->acc_pct > 100)
                    pr->acc_pct = 100;
            }
            continue;
        }

        /* Passed ICDs, apply effect at event time */
        if (pr->def.icd_global_ms > 0.0)
            pr->last_global_ms = now_ms;
        if (pr->def.use_smoothing)
        {
            /* On trigger, bleed accumulator: subtract base chance to avoid runaway */
            pr->acc_pct -= pr->def.chance_pct;
            if (pr->acc_pct < 0)
                pr->acc_pct = 0;
        }
        /* Record recent trigger for dynamic scaling */
        if (pr->recent_count < (int) (sizeof(pr->recent_ms) / sizeof(pr->recent_ms[0])))
            pr->recent_ms[pr->recent_count++] = now_ms;
        else
        {
            /* simple shift if full */
            memmove(&pr->recent_ms[0], &pr->recent_ms[1],
                    (pr->recent_count - 1) * (int) sizeof(pr->recent_ms[0]));
            pr->recent_ms[pr->recent_count - 1] = now_ms;
        }
        if (pr->def.effect_spec_id >= 0)
        {
            /* Phase 7.4: anti-loop guard using cycle signature */
            unsigned long long apply_sig =
                mix_u64(base_sig, (unsigned long long) pr->def.effect_spec_id);
            int seen = 0;
            for (int s = 0; s < g_cycle_seen_count; ++s)
            {
                if (g_cycle_seen[s] == apply_sig)
                {
                    seen = 1;
                    break;
                }
            }
            if (seen)
            {
                ROGUE_LOG_WARN(
                    "[skills_procs] cycle detected (event %u, effect %d); applying blocked",
                    (unsigned) ev->type_id, pr->def.effect_spec_id);
            }
            else
            {
                if (g_cycle_seen_count < ROGUE_PROC_CYCLE_SEEN_MAX)
                    g_cycle_seen[g_cycle_seen_count++] = apply_sig;
                /* TEMP: debug output to help diagnose unit test regression for Phase 7.2 ICD */
                fprintf(stderr, "[skills_procs] apply effect id=%d at %.3f ms (event=%u)\n",
                        pr->def.effect_spec_id, now_ms, (unsigned) ev->type_id);
                rogue_effect_apply(pr->def.effect_spec_id, now_ms);
            }
        }
    }
    g_loop_depth--;
    if (g_loop_depth < 0)
        g_loop_depth = 0;
    return true;
}

void rogue_skills_procs_init(void)
{
    memset(g_procs, 0, sizeof(g_procs));
    g_proc_count = 0;
    /* Subscribe a single callback per interesting event; we use it to fan-out to registered procs.
     */
    /* We'll subscribe lazily on first registration to keep noise low. */
}

void rogue_skills_procs_shutdown(void)
{
    /* Unsubscribe any registered subs */
    for (int i = 0; i < g_proc_count; ++i)
    {
        if (g_procs[i].in_use && g_procs[i].sub_id)
        {
            rogue_event_unsubscribe(g_procs[i].sub_id);
        }
    }
    memset(g_procs, 0, sizeof(g_procs));
    g_proc_count = 0;
}

void rogue_skills_procs_reset(void)
{
    rogue_skills_procs_shutdown();
    rogue_skills_procs_init();
}

static uint32_t ensure_subscription(RogueEventTypeId type)
{
    /* Avoid duplicate subscriptions: check existing ones */
    for (int i = 0; i < g_proc_count; ++i)
    {
        if (g_procs[i].in_use && g_procs[i].def.event_type == type && g_procs[i].sub_id)
        {
            return g_procs[i].sub_id;
        }
    }
    return rogue_event_subscribe(type, proc_event_cb, NULL, 0x50524F43 /* 'PROC' */);
}

int rogue_skills_proc_register(const RogueProcDef* def)
{
    if (!def)
        return -1;
    if (g_proc_count >= ROGUE_MAX_PROCS)
    {
        ROGUE_LOG_ERROR("Proc registry full");
        return -1;
    }
    RogueProcRuntime* pr = &g_procs[g_proc_count];
    memset(pr, 0, sizeof(*pr));
    pr->def = *def;
    /* Back-compat: if chance not specified by caller (zero-initialized struct), treat as 100% */
    if (pr->def.chance_pct <= 0)
        pr->def.chance_pct = 100;
    if (pr->def.chance_pct > 100)
        pr->def.chance_pct = 100;
    pr->sub_id = ensure_subscription(def->event_type);
    fprintf(stderr,
            "[skills_procs] register proc idx=%d evt=%u sub_id=%u chance=%d icdG=%.1f icdT=%.1f\n",
            g_proc_count, (unsigned) def->event_type, (unsigned) pr->sub_id, pr->def.chance_pct,
            pr->def.icd_global_ms, pr->def.icd_per_target_ms);
    if (!pr->sub_id)
    {
        ROGUE_LOG_ERROR("Failed to subscribe proc to event type %u", (unsigned) def->event_type);
        return -1;
    }
    pr->in_use = 1;
    pr->last_global_ms = -1e12; /* so first trigger passes */
    pr->acc_pct = 0;
    g_proc_count++;
    return g_proc_count - 1;
}

int rogue_skills_proc_count(void) { return g_proc_count; }

int rogue_skills_proc_get_def(int index, RogueProcDef* out)
{
    if (index < 0 || index >= g_proc_count || !out)
        return 0;
    *out = g_procs[index].def;
    return 1;
}
