/**
 * @file buffs.c
 * @brief Implementation of the timed buff system with handle-based pool management.
 *
 * This module provides a comprehensive buff system supporting various buff types,
 * stacking rules, diminishing returns for crowd control effects, and audio-visual
 * feedback. It uses a fixed-capacity handle-based pool for efficient memory management
 * and ABA-safe handle validation.
 *
 * Key features:
 * - Handle-based API for safe buff references
 * - Multiple stacking behaviors (unique, refresh, extend, add, multiply, replace-if-stronger)
 * - Diminishing returns for crowd control effects (stun, root, slow)
 * - Anti-oscillation dampening to prevent buff spam
 * - Audio-visual feedback on buff gain/expire
 * - Category-based buff classification
 *
 * @note Phase 4.5: Added diminishing returns for CC categories
 * @note Phase 10.3: Enhanced stacking rules
 * @note Phase 10.4: Anti-oscillation dampening
 */

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

/**
 * @brief Ensures the buff system is initialized.
 *
 * This function provides lazy initialization of the buff pool and global state.
 * It preserves legacy behavior where initialization wasn't explicitly called in tests.
 * Safe to call multiple times.
 */
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

/**
 * @brief Creates a buff handle from an internal pool index.
 *
 * Packs the generation counter (high 16 bits) and index (low 16 bits) into a 32-bit handle
 * for ABA-safe validation.
 *
 * @param idx The internal pool index (0 to ROGUE_MAX_ACTIVE_BUFFS-1)
 * @return A valid RogueBuffHandle
 */
static inline RogueBuffHandle _make_handle(int idx)
{
    /* pack gen (hi16) | index (lo16) */
    uint16_t gen = g_buffs[idx]._gen ? g_buffs[idx]._gen : 1;
    return ((uint32_t) gen << 16) | (uint32_t) (idx & 0xFFFF);
}
/**
 * @brief Extracts the pool index from a buff handle.
 *
 * @param h The buff handle
 * @return The internal pool index (0 to ROGUE_MAX_ACTIVE_BUFFS-1)
 */
static inline int _handle_index(RogueBuffHandle h) { return (int) (h & 0xFFFF); }

/**
 * @brief Extracts the generation counter from a buff handle.
 *
 * @param h The buff handle
 * @return The generation counter (high 16 bits)
 */
static inline uint16_t _handle_gen(RogueBuffHandle h) { return (uint16_t) (h >> 16); }
/**
 * @brief Validates a buff handle and returns the corresponding pool index.
 *
 * Performs bounds checking, active status verification, and generation counter validation
 * to ensure the handle is still valid and hasn't been recycled.
 *
 * @param h The buff handle to validate
 * @return The pool index if valid, -1 if invalid
 */
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
/**
 * @brief Allocates a free slot from the buff pool.
 *
 * Uses a free-list to efficiently find and allocate available buff slots.
 *
 * @return The allocated pool index, or -1 if pool is full
 */
static int _alloc_slot(void)
{
    if (g_free_head < 0)
        return -1;
    int idx = g_free_head;
    g_free_head = g_buffs[idx]._next_free;
    g_buffs[idx]._next_free = -1;
    return idx;
}
/**
 * @brief Returns a buff slot to the free pool.
 *
 * Deactivates the buff, increments the generation counter for ABA safety,
 * and adds the slot back to the free-list.
 *
 * @param idx The pool index to free (must be valid)
 */
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

/**
 * @brief Initializes the buff system.
 *
 * Sets up the free-list, zeros all buff slots, and initializes global state.
 * Safe to call multiple times - subsequent calls are no-ops.
 */
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

/**
 * @brief Updates the buff system, expiring buffs that have reached their end time.
 *
 * Iterates through all active buffs and expires those whose end_ms is less than
 * or equal to the current time. Triggers audio-visual feedback and expiration
 * callbacks for expired buffs.
 *
 * @param now_ms Current time in milliseconds
 */
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

/**
 * @brief Applies a buff with the specified parameters.
 *
 * Attempts to apply a buff of the given type, magnitude, and duration. Handles stacking
 * rules, diminishing returns for crowd control effects, anti-oscillation dampening,
 * and audio-visual feedback.
 *
 * @param type The type of buff to apply
 * @param magnitude The strength/intensity of the buff
 * @param duration_ms How long the buff should last in milliseconds
 * @param now_ms Current time in milliseconds
 * @param rule How to handle stacking with existing buffs of the same type
 * @param snapshot Whether the buff magnitude should be snapshotted (immutable)
 * @return 1 if the buff was successfully applied, 0 otherwise
 */
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

/**
 * @brief Applies a buff and returns a handle to it.
 *
 * Handle-returning variant of rogue_buffs_apply. Returns a RogueBuffHandle that can be
 * used for subsequent operations on the buff, or ROGUE_BUFF_INVALID_HANDLE on failure.
 *
 * @param type The type of buff to apply
 * @param magnitude The strength/intensity of the buff
 * @param duration_ms How long the buff should last in milliseconds
 * @param now_ms Current time in milliseconds
 * @param rule How to handle stacking with existing buffs of the same type
 * @param snapshot Whether the buff magnitude should be snapshotted (immutable)
 * @return A valid handle to the buff, or ROGUE_BUFF_INVALID_HANDLE on failure
 */
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

/**
 * @brief Refreshes an existing buff via its handle.
 *
 * Applies new parameters to an existing buff identified by handle, following the
 * specified stacking rule. The snapshot flag is ignored (immutable post-creation).
 *
 * @param h Handle to the buff to refresh
 * @param magnitude New magnitude value
 * @param duration_ms New duration in milliseconds
 * @param now_ms Current time in milliseconds
 * @param rule Stacking rule to apply
 * @param snapshot Ignored (snapshot status cannot be changed)
 * @return 1 if refresh succeeded, 0 if handle invalid
 */
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

/**
 * @brief Removes a buff via its handle.
 *
 * Immediately expires a buff identified by handle, triggering audio-visual feedback
 * and expiration callbacks as if it had expired naturally.
 *
 * @param h Handle to the buff to remove
 * @param now_ms Current time (reserved for future use)
 * @return 1 if removal succeeded, 0 if handle invalid
 */
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

/**
 * @brief Queries buff data via its handle.
 *
 * Retrieves the current state of a buff identified by handle.
 *
 * @param h Handle to the buff to query
 * @param out Pointer to RogueBuff struct to fill with buff data
 * @return 1 if query succeeded, 0 if handle invalid or out is NULL
 */
int rogue_buffs_query_h(RogueBuffHandle h, RogueBuff* out)
{
    _ensure_init();
    int idx = _validate_handle(h);
    if (idx < 0 || !out)
        return 0;
    *out = g_buffs[idx];
    return 1;
}

/**
 * @brief Gets the total strength bonus from all active strength buffs.
 *
 * Sums the magnitudes of all active ROGUE_BUFF_STAT_STRENGTH buffs.
 * Used for stat calculation integration.
 *
 * @return Total strength bonus value
 */
int rogue_buffs_strength_bonus(void)
{
    _ensure_init();
    int total = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == ROGUE_BUFF_STAT_STRENGTH)
            total += g_buffs[i].magnitude;
    return total;
}
/**
 * @brief Sets the minimum interval between same-type buff applications.
 *
 * Configures anti-oscillation dampening to prevent buff spam. Buffs of the same
 * type applied within this interval will be rejected.
 *
 * @param min_interval_ms Minimum milliseconds between applications (clamped to >= 0)
 */
void rogue_buffs_set_dampening(double min_interval_ms)
{
    _ensure_init();
    if (min_interval_ms < 0)
        min_interval_ms = 0;
    g_min_reapply_interval_ms = min_interval_ms;
}

/**
 * @brief Gets the total magnitude of all active buffs of a specific type.
 *
 * Sums the magnitudes of all active buffs matching the specified type.
 *
 * @param type The buff type to query
 * @return Total magnitude of all matching active buffs
 */
int rogue_buffs_get_total(RogueBuffType type)
{
    _ensure_init();
    int total = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == type)
            total += g_buffs[i].magnitude;
    return total;
}

/**
 * @brief Gets the number of currently active buffs.
 *
 * @return The count of active buff slots
 */
int rogue_buffs_active_count(void)
{
    _ensure_init();
    int c = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active)
            c++;
    return c;
}
/**
 * @brief Gets the Nth active buff by index.
 *
 * Retrieves buff data for the Nth active buff (0-based indexing).
 * Useful for iterating through all active buffs.
 *
 * @param index The index of the active buff to retrieve (0 to active_count-1)
 * @param out Pointer to RogueBuff struct to fill with buff data
 * @return 1 if retrieval succeeded, 0 if index out of range or out is NULL
 */
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
/**
 * @brief Creates a snapshot of all active buffs.
 *
 * Copies up to 'max' active buffs into the provided array, pruning any expired buffs
 * in the process. Useful for persistence and UI display.
 *
 * @param out Array to store buff snapshots
 * @param max Maximum number of buffs to copy
 * @param now_ms Current time for expiration checking
 * @return Number of buffs copied to the array
 */
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

/**
 * @brief Sets the callback function for buff expiration events.
 *
 * Registers a callback that will be invoked whenever a buff expires naturally
 * or is manually removed. The callback receives the buff type and final magnitude.
 *
 * @param cb Function pointer to the expiration callback, or NULL to disable
 */
void rogue_buffs_set_on_expire(RogueBuffExpireFn cb) { g_on_expire = cb; }

/* Phase 4.3: Category helpers. Keep a simple mapping for built-in types. */
/**
 * @brief Gets the category flags for a buff type.
 *
 * Returns a bitmask of category flags (offensive, defensive, movement, utility)
 * and crowd control sub-flags (stun, root, slow) for the given buff type.
 *
 * @param type The buff type to query
 * @return Bitmask of RogueBuffCategoryFlags
 */
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
/**
 * @brief Sets the diminishing returns window duration.
 *
 * Configures how long the DR window lasts for crowd control effects.
 * Within this window, repeated CC applications of the same type have reduced effectiveness.
 *
 * @param ms Window duration in milliseconds (clamped to >= 0)
 */
void rogue_buffs_set_dr_window_ms(double ms)
{
    _ensure_init();
    if (ms < 0)
        ms = 0;
    g_dr_window_ms = ms;
}
/**
 * @brief Resets the diminishing returns state.
 *
 * Clears all DR counters and windows, effectively resetting CC effectiveness
 * to 100% for all categories.
 */
void rogue_buffs_reset_dr_state(void)
{
    _ensure_init();
    g_dr_stun_end_ms = g_dr_root_end_ms = g_dr_slow_end_ms = 0.0;
    g_dr_stun_count = g_dr_root_count = g_dr_slow_count = 0;
}
