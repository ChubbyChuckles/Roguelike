#include "buffs.h"
#include "../audio_vfx/effects.h" /* Phase 5.4: buff gain/expire cues */
#include "../core/app/app_state.h"
#include <string.h>

/* Handle-based pool (backed by fixed capacity, free-list) */
#define ROGUE_MAX_ACTIVE_BUFFS 32
static RogueBuff g_buffs[ROGUE_MAX_ACTIVE_BUFFS];
static int g_free_head = -1;                    /* index of first free slot */
static double g_min_reapply_interval_ms = 50.0; /* default dampening window */
static RogueBuffExpireFn g_on_expire = NULL;
/* Auto-init guard to preserve legacy behavior where init wasn't explicitly called in tests */
static int g_initialized = 0;
/* Phase 4.5: DR tracker for CC categories (per-target global for now) */
static double g_dr_window_ms = 15000.0; /* 15s default DR window */
static double g_dr_stun_end_ms = 0.0;
static double g_dr_root_end_ms = 0.0;
static double g_dr_slow_end_ms = 0.0;
/* active stacks within window: 0 -> 1.0, 1 -> 0.5, 2 -> 0.25, 3+ -> 0.0 */
static int g_dr_stun_count = 0;
static int g_dr_root_count = 0;
static int g_dr_slow_count = 0;

static inline void _ensure_init(void)
{
    if (!g_initialized)
    {
        /* initialize free list and defaults */
        memset(g_buffs, 0, sizeof g_buffs);
        g_free_head = 0;
        for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        {
            g_buffs[i]._gen = 1; /* non-zero */
            g_buffs[i]._next_free = (i + 1 < ROGUE_MAX_ACTIVE_BUFFS) ? (i + 1) : -1;
        }
        g_min_reapply_interval_ms = 50.0;
        g_on_expire = NULL;
        g_initialized = 1;
        g_dr_window_ms = 15000.0;
        g_dr_stun_end_ms = g_dr_root_end_ms = g_dr_slow_end_ms = 0.0;
        g_dr_stun_count = g_dr_root_count = g_dr_slow_count = 0;
    }
}

static inline RogueBuffHandle _make_handle(int idx)
{
    /* pack gen (hi16) | index (lo16) */
    uint16_t gen = g_buffs[idx]._gen ? g_buffs[idx]._gen : 1;
    return ((uint32_t) gen << 16) | (uint32_t) (idx & 0xFFFF);
}
static inline int _handle_index(RogueBuffHandle h) { return (int) (h & 0xFFFF); }
static inline uint16_t _handle_gen(RogueBuffHandle h) { return (uint16_t) (h >> 16); }
static int _validate_handle(RogueBuffHandle h)
{
    if (h == ROGUE_BUFF_INVALID_HANDLE)
        return -1;
    int idx = _handle_index(h);
    uint16_t gen = _handle_gen(h);
    if (idx < 0 || idx >= ROGUE_MAX_ACTIVE_BUFFS)
        return -1;
    if (!g_buffs[idx].active)
        return -1;
    if (g_buffs[idx]._gen != gen)
        return -1;
    return idx;
}
static int _alloc_slot(void)
{
    if (g_free_head < 0)
        return -1;
    int idx = g_free_head;
    g_free_head = g_buffs[idx]._next_free;
    g_buffs[idx]._next_free = -1;
    return idx;
}
static void _free_slot(int idx)
{
    if (idx < 0 || idx >= ROGUE_MAX_ACTIVE_BUFFS)
        return;
    g_buffs[idx].active = 0;
    /* bump generation (avoid 0) */
    g_buffs[idx]._gen = (uint16_t) (g_buffs[idx]._gen + 1);
    if (g_buffs[idx]._gen == 0)
        g_buffs[idx]._gen = 1;
    g_buffs[idx]._next_free = g_free_head;
    g_free_head = idx;
}

void rogue_buffs_init(void)
{
    memset(g_buffs, 0, sizeof g_buffs);
    g_free_head = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
    {
        g_buffs[i]._gen = 1;
        g_buffs[i]._next_free = (i + 1 < ROGUE_MAX_ACTIVE_BUFFS) ? (i + 1) : -1;
    }
    g_min_reapply_interval_ms = 50.0;
    g_on_expire = NULL;
    g_initialized = 1;
    g_dr_window_ms = 15000.0;
    g_dr_stun_end_ms = g_dr_root_end_ms = g_dr_slow_end_ms = 0.0;
    g_dr_stun_count = g_dr_root_count = g_dr_slow_count = 0;
}

void rogue_buffs_update(double now_ms)
{
    _ensure_init();
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && now_ms >= g_buffs[i].end_ms)
        {
            /* FX: buff expire cue before deactivating */
            char key[48];
            snprintf(key, sizeof key, "buff/%d/expire", (int) g_buffs[i].type);
            rogue_fx_trigger_event(key, g_app.player.base.pos.x, g_app.player.base.pos.y);
            if (g_on_expire)
                g_on_expire(g_buffs[i].type, g_buffs[i].magnitude);
            _free_slot(i);
        }
}

int rogue_buffs_apply(RogueBuffType type, int magnitude, double duration_ms, double now_ms,
                      RogueBuffStackRule rule, int snapshot)
{
    _ensure_init();
    if (magnitude <= 0 || duration_ms <= 0)
        return 0;
    if (now_ms < 0)
        now_ms = 0;
    if (rule < 0 || rule > ROGUE_BUFF_STACK_REPLACE_IF_STRONGER)
        rule = ROGUE_BUFF_STACK_ADD;
    /* dampening */
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == type && now_ms < g_buffs[i].end_ms)
        {
            if (now_ms - g_buffs[i].last_apply_ms < g_min_reapply_interval_ms)
                return 0;
            break;
        }
    /* Phase 4.5: DR adjustment for CC durations (applies to both stack-with-existing and new) */
    double effective_duration = duration_ms;
    uint32_t cats = rogue_buffs_type_categories(type);
    if (type == ROGUE_BUFF_CC_STUN || type == ROGUE_BUFF_CC_ROOT || type == ROGUE_BUFF_CC_SLOW ||
        (cats & (ROGUE_BUFF_CCFLAG_STUN | ROGUE_BUFF_CCFLAG_ROOT | ROGUE_BUFF_CCFLAG_SLOW)))
    {
        if (type == ROGUE_BUFF_CC_STUN || (cats & ROGUE_BUFF_CCFLAG_STUN))
        {
            if (now_ms > g_dr_stun_end_ms)
            {
                g_dr_stun_count = 0;
                g_dr_stun_end_ms = now_ms + g_dr_window_ms; /* anchor window at first apply */
            }
            double factor = (g_dr_stun_count == 0)   ? 1.0
                            : (g_dr_stun_count == 1) ? 0.5
                            : (g_dr_stun_count == 2) ? 0.25
                                                     : 0.0;
            effective_duration *= factor;
            g_dr_stun_count++;
        }
        else if (type == ROGUE_BUFF_CC_ROOT || (cats & ROGUE_BUFF_CCFLAG_ROOT))
        {
            if (now_ms > g_dr_root_end_ms)
            {
                g_dr_root_count = 0;
                g_dr_root_end_ms = now_ms + g_dr_window_ms; /* anchor window */
            }
            double factor = (g_dr_root_count == 0)   ? 1.0
                            : (g_dr_root_count == 1) ? 0.5
                            : (g_dr_root_count == 2) ? 0.25
                                                     : 0.0;
            effective_duration *= factor;
            g_dr_root_count++;
        }
        else if (type == ROGUE_BUFF_CC_SLOW || (cats & ROGUE_BUFF_CCFLAG_SLOW))
        {
            if (now_ms > g_dr_slow_end_ms)
            {
                g_dr_slow_count = 0;
                g_dr_slow_end_ms = now_ms + g_dr_window_ms; /* anchor window */
            }
            double factor = (g_dr_slow_count == 0)   ? 1.0
                            : (g_dr_slow_count == 1) ? 0.5
                            : (g_dr_slow_count == 2) ? 0.25
                                                     : 0.0;
            effective_duration *= factor;
            g_dr_slow_count++;
        }
        if (effective_duration <= 0)
            return 1; /* counted for DR but zero duration -> no buff record */
    }
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == type && now_ms < g_buffs[i].end_ms)
        {
            RogueBuff* b = &g_buffs[i];
            b->last_apply_ms = now_ms;
            switch (rule)
            {
            case ROGUE_BUFF_STACK_UNIQUE:
                return 0; /* ignore */
            case ROGUE_BUFF_STACK_REFRESH:
                if (magnitude > b->magnitude)
                    b->magnitude = magnitude;
                b->end_ms = now_ms + effective_duration;
                return 1;
            case ROGUE_BUFF_STACK_EXTEND:
                b->end_ms += effective_duration;
                if (b->end_ms < now_ms + effective_duration)
                    b->end_ms = now_ms + effective_duration;
                if (b->magnitude < magnitude)
                    b->magnitude = magnitude;
                return 1;
            case ROGUE_BUFF_STACK_ADD:
            default:
                b->magnitude += magnitude;
                if (b->magnitude > 999)
                    b->magnitude = 999;
                if (now_ms + effective_duration > b->end_ms)
                    b->end_ms = now_ms + effective_duration;
                return 1;
            case ROGUE_BUFF_STACK_MULTIPLY:
            {
                /* Interpret incoming magnitude as percent (e.g., 110 = +10%). Clamp floor at 1%. */
                int pct = magnitude;
                if (pct < 1)
                    pct = 1;
                long long nm = (long long) b->magnitude * (long long) pct;
                nm /= 100;
                if (nm < 0)
                    nm = 0;
                if (nm > 999)
                    nm = 999;
                b->magnitude = (int) nm;
                /* Extend duration if incoming provides longer remaining. */
                double new_end = now_ms + effective_duration;
                if (new_end > b->end_ms)
                    b->end_ms = new_end;
                return 1;
            }
            case ROGUE_BUFF_STACK_REPLACE_IF_STRONGER:
                if (magnitude > b->magnitude)
                    b->magnitude = magnitude;
                /* Always take the longer remaining duration */
                if (now_ms + effective_duration > b->end_ms)
                    b->end_ms = now_ms + effective_duration;
                return 1;
            }
        }
    /* allocate new slot */
    {
        int idx = _alloc_slot();
        if (idx < 0)
            return 0;
        /* For multiplicative stacking, if there's no existing buff to multiply, do nothing. */
        if (rule == ROGUE_BUFF_STACK_MULTIPLY)
        {
            _free_slot(idx);
            return 0;
        }
        g_buffs[idx].active = 1;
        g_buffs[idx].type = type;
        g_buffs[idx].magnitude = magnitude;
        g_buffs[idx].end_ms = now_ms + effective_duration;
        g_buffs[idx].snapshot = snapshot ? 1 : 0;
        g_buffs[idx].stack_rule = rule;
        g_buffs[idx].last_apply_ms = now_ms;
        g_buffs[idx].categories = cats;
        /* FX: buff gain cue */
        {
            char key[48];
            snprintf(key, sizeof key, "buff/%d/gain", (int) type);
            rogue_fx_trigger_event(key, g_app.player.base.pos.x, g_app.player.base.pos.y);
        }
        return 1;
    }
}

RogueBuffHandle rogue_buffs_apply_h(RogueBuffType type, int magnitude, double duration_ms,
                                    double now_ms, RogueBuffStackRule rule, int snapshot)
{
    _ensure_init();
    if (magnitude <= 0 || duration_ms <= 0)
        return ROGUE_BUFF_INVALID_HANDLE;
    if (now_ms < 0)
        now_ms = 0;
    if (rule < 0 || rule > ROGUE_BUFF_STACK_REPLACE_IF_STRONGER)
        rule = ROGUE_BUFF_STACK_ADD;

    /* dampening: see if an active same-type exists and was applied too recently */
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == type && now_ms < g_buffs[i].end_ms)
        {
            if (now_ms - g_buffs[i].last_apply_ms < g_min_reapply_interval_ms)
                return ROGUE_BUFF_INVALID_HANDLE;
            break;
        }
    /* DR compute for CC (applies to both stack-with-existing and new). If zero, do nothing and
     * return invalid handle. */
    double effective_duration = duration_ms;
    uint32_t cats = rogue_buffs_type_categories(type);
    if (type == ROGUE_BUFF_CC_STUN || type == ROGUE_BUFF_CC_ROOT || type == ROGUE_BUFF_CC_SLOW ||
        (cats & (ROGUE_BUFF_CCFLAG_STUN | ROGUE_BUFF_CCFLAG_ROOT | ROGUE_BUFF_CCFLAG_SLOW)))
    {
        if (type == ROGUE_BUFF_CC_STUN || (cats & ROGUE_BUFF_CCFLAG_STUN))
        {
            if (now_ms > g_dr_stun_end_ms)
            {
                g_dr_stun_count = 0;
                g_dr_stun_end_ms = now_ms + g_dr_window_ms;
            }
            double factor = (g_dr_stun_count == 0)   ? 1.0
                            : (g_dr_stun_count == 1) ? 0.5
                            : (g_dr_stun_count == 2) ? 0.25
                                                     : 0.0;
            effective_duration *= factor;
            g_dr_stun_count++;
        }
        else if (type == ROGUE_BUFF_CC_ROOT || (cats & ROGUE_BUFF_CCFLAG_ROOT))
        {
            if (now_ms > g_dr_root_end_ms)
            {
                g_dr_root_count = 0;
                g_dr_root_end_ms = now_ms + g_dr_window_ms;
            }
            double factor = (g_dr_root_count == 0)   ? 1.0
                            : (g_dr_root_count == 1) ? 0.5
                            : (g_dr_root_count == 2) ? 0.25
                                                     : 0.0;
            effective_duration *= factor;
            g_dr_root_count++;
        }
        else if (type == ROGUE_BUFF_CC_SLOW || (cats & ROGUE_BUFF_CCFLAG_SLOW))
        {
            if (now_ms > g_dr_slow_end_ms)
            {
                g_dr_slow_count = 0;
                g_dr_slow_end_ms = now_ms + g_dr_window_ms;
            }
            double factor = (g_dr_slow_count == 0)   ? 1.0
                            : (g_dr_slow_count == 1) ? 0.5
                            : (g_dr_slow_count == 2) ? 0.25
                                                     : 0.0;
            effective_duration *= factor;
            g_dr_slow_count++;
        }
        if (effective_duration <= 0)
            return ROGUE_BUFF_INVALID_HANDLE;
    }
    /* try stack with existing */
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == type && now_ms < g_buffs[i].end_ms)
        {
            RogueBuff* b = &g_buffs[i];
            b->last_apply_ms = now_ms;
            switch (rule)
            {
            case ROGUE_BUFF_STACK_UNIQUE:
                return ROGUE_BUFF_INVALID_HANDLE;
            case ROGUE_BUFF_STACK_REFRESH:
                if (magnitude > b->magnitude)
                    b->magnitude = magnitude;
                b->end_ms = now_ms + effective_duration;
                return _make_handle(i);
            case ROGUE_BUFF_STACK_EXTEND:
                b->end_ms += effective_duration;
                if (b->end_ms < now_ms + effective_duration)
                    b->end_ms = now_ms + effective_duration;
                if (b->magnitude < magnitude)
                    b->magnitude = magnitude;
                return _make_handle(i);
            case ROGUE_BUFF_STACK_ADD:
            default:
                b->magnitude += magnitude;
                if (b->magnitude > 999)
                    b->magnitude = 999;
                if (now_ms + effective_duration > b->end_ms)
                    b->end_ms = now_ms + effective_duration;
                return _make_handle(i);
            case ROGUE_BUFF_STACK_MULTIPLY:
            {
                int pct = magnitude;
                if (pct < 1)
                    pct = 1;
                long long nm = (long long) b->magnitude * (long long) pct;
                nm /= 100;
                if (nm < 0)
                    nm = 0;
                if (nm > 999)
                    nm = 999;
                b->magnitude = (int) nm;
                double new_end = now_ms + effective_duration;
                if (new_end > b->end_ms)
                    b->end_ms = new_end;
                return _make_handle(i);
            }
            case ROGUE_BUFF_STACK_REPLACE_IF_STRONGER:
                if (magnitude > b->magnitude)
                    b->magnitude = magnitude;
                if (now_ms + effective_duration > b->end_ms)
                    b->end_ms = now_ms + effective_duration;
                return _make_handle(i);
            }
        }
    /* new */
    int idx = _alloc_slot();
    if (idx < 0)
        return ROGUE_BUFF_INVALID_HANDLE;
    if (rule == ROGUE_BUFF_STACK_MULTIPLY)
    {
        _free_slot(idx);
        return ROGUE_BUFF_INVALID_HANDLE;
    }
    g_buffs[idx].active = 1;
    g_buffs[idx].type = type;
    g_buffs[idx].magnitude = magnitude;
    g_buffs[idx].end_ms = now_ms + effective_duration;
    g_buffs[idx].snapshot = snapshot ? 1 : 0;
    g_buffs[idx].stack_rule = rule;
    g_buffs[idx].last_apply_ms = now_ms;
    g_buffs[idx].categories = cats;
    {
        char key[48];
        snprintf(key, sizeof key, "buff/%d/gain", (int) type);
        rogue_fx_trigger_event(key, g_app.player.base.pos.x, g_app.player.base.pos.y);
    }
    return _make_handle(idx);
}

int rogue_buffs_refresh_h(RogueBuffHandle h, int magnitude, double duration_ms, double now_ms,
                          RogueBuffStackRule rule, int snapshot)
{
    _ensure_init();
    int idx = _validate_handle(h);
    if (idx < 0)
        return 0;
    (void) snapshot; /* snapshot immutable post-creation in this minimal API */
    return rogue_buffs_apply(g_buffs[idx].type, magnitude, duration_ms, now_ms, rule,
                             g_buffs[idx].snapshot);
}

int rogue_buffs_remove_h(RogueBuffHandle h, double now_ms)
{
    _ensure_init();
    int idx = _validate_handle(h);
    if (idx < 0)
        return 0;
    (void) now_ms; /* reserved */
    /* FX + callback mirror natural expiry */
    char key[48];
    snprintf(key, sizeof key, "buff/%d/expire", (int) g_buffs[idx].type);
    rogue_fx_trigger_event(key, g_app.player.base.pos.x, g_app.player.base.pos.y);
    if (g_on_expire)
        g_on_expire(g_buffs[idx].type, g_buffs[idx].magnitude);
    _free_slot(idx);
    return 1;
}

int rogue_buffs_query_h(RogueBuffHandle h, RogueBuff* out)
{
    _ensure_init();
    int idx = _validate_handle(h);
    if (idx < 0 || !out)
        return 0;
    *out = g_buffs[idx];
    return 1;
}

int rogue_buffs_strength_bonus(void)
{
    _ensure_init();
    int total = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == ROGUE_BUFF_STAT_STRENGTH)
            total += g_buffs[i].magnitude;
    return total;
}
void rogue_buffs_set_dampening(double min_interval_ms)
{
    _ensure_init();
    if (min_interval_ms < 0)
        min_interval_ms = 0;
    g_min_reapply_interval_ms = min_interval_ms;
}

int rogue_buffs_get_total(RogueBuffType type)
{
    _ensure_init();
    int total = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == type)
            total += g_buffs[i].magnitude;
    return total;
}

int rogue_buffs_active_count(void)
{
    _ensure_init();
    int c = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active)
            c++;
    return c;
}
int rogue_buffs_get_active(int index, RogueBuff* out)
{
    _ensure_init();
    if (!out)
        return 0;
    int c = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active)
        {
            if (c == index)
            {
                *out = g_buffs[i];
                return 1;
            }
            c++;
        }
    return 0;
}
int rogue_buffs_snapshot(RogueBuff* out, int max, double now_ms)
{
    _ensure_init();
    if (!out || max <= 0)
        return 0;
    int c = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS && c < max; i++)
    {
        if (g_buffs[i].active)
        {
            if (now_ms >= g_buffs[i].end_ms)
            {
                if (g_on_expire)
                    g_on_expire(g_buffs[i].type, g_buffs[i].magnitude);
                _free_slot(i);
                continue;
            }
            out[c++] = g_buffs[i];
        }
    }
    return c;
}

void rogue_buffs_set_on_expire(RogueBuffExpireFn cb) { g_on_expire = cb; }

/* Phase 4.3: Category helpers. Keep a simple mapping for built-in types. */
uint32_t rogue_buffs_type_categories(RogueBuffType type)
{
    switch (type)
    {
    case ROGUE_BUFF_POWER_STRIKE:
        return ROGUE_BUFF_CAT_OFFENSIVE;
    case ROGUE_BUFF_STAT_STRENGTH:
        return ROGUE_BUFF_CAT_UTILITY;
    case ROGUE_BUFF_CC_STUN:
        return ROGUE_BUFF_CCFLAG_STUN | ROGUE_BUFF_CAT_UTILITY;
    case ROGUE_BUFF_CC_ROOT:
        return ROGUE_BUFF_CCFLAG_ROOT | ROGUE_BUFF_CAT_UTILITY;
    case ROGUE_BUFF_CC_SLOW:
        return ROGUE_BUFF_CCFLAG_SLOW | ROGUE_BUFF_CAT_MOVEMENT;
    default:
        return 0;
    }
}

/* Phase 4.5: DR API */
void rogue_buffs_set_dr_window_ms(double ms)
{
    _ensure_init();
    if (ms < 0)
        ms = 0;
    g_dr_window_ms = ms;
}
void rogue_buffs_reset_dr_state(void)
{
    _ensure_init();
    g_dr_stun_end_ms = g_dr_root_end_ms = g_dr_slow_end_ms = 0.0;
    g_dr_stun_count = g_dr_root_count = g_dr_slow_count = 0;
}
