/**
 * @file combat_events.c
 * @brief Isolated damage event ring buffer and critical hit layering system
 *
 * This module implements a ring buffer for storing damage events and manages critical hit
 * layering modes. The separation from main combat logic enables faster incremental builds
 * by keeping frequently modified strike logic out of the hot include path.
 *
 * Key features:
 * - Ring buffer storage for damage events with configurable capacity
 * - Critical hit layering mode (pre/post-mitigation)
 * - Test hooks for forcing critical hits
 * - Observer pattern support for damage event monitoring
 * - Compile-time capacity validation
 *
 * @note Phase 2.7: Damage event ring buffer implementation
 * @note Separation keeps frequently modified strike logic out of hot include path for faster
 * incremental builds
 */

#include "combat.h"
#if defined(_WIN32) && defined(ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS)
/* Pull in the suppression TU from the static lib and install early for tests. */
extern int rogue_win_disable_error_dialogs_anchor;
extern void rogue_win_disable_error_dialogs_install_if_needed(void);
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_MSC_VER)
#define STATIC_ASSERT(expr, msg) typedef char static_assert_##msg[(expr) ? 1 : -1]
#else
#define STATIC_ASSERT(expr, msg) _Static_assert(expr, #msg)
#endif
/** @brief Compile-time validation that damage event capacity is reasonable */
STATIC_ASSERT(ROGUE_DAMAGE_EVENT_CAP <= 256, damage_event_cap_reasonable);

/** @brief Ring buffer storage for damage events (Phase 2.7) */
RogueDamageEvent g_damage_events[ROGUE_DAMAGE_EVENT_CAP];
/** @brief Current head position in the damage event ring buffer */
int g_damage_event_head = 0;
/** @brief Total number of damage events recorded (can exceed buffer capacity) */
int g_damage_event_total = 0;
/** @brief Critical hit layering mode: 0=pre-mitigation (legacy), 1=post-mitigation */
int g_crit_layering_mode = 0;
/** @brief Test hook for forcing critical hits: -1=RNG, 0=force non-crit, 1=force crit */
int g_force_crit_mode = -1;

/**
 * @brief Base implementation for recording damage events to the ring buffer
 *
 * Records a damage event with all relevant combat information to the global ring buffer.
 * This base implementation is separated so the observer module can wrap it if needed.
 * On Windows with error dialog suppression enabled, ensures the suppression is installed.
 *
 * @param attack_id Unique identifier for the attack that caused this damage
 * @param dmg_type Type of damage (physical, magical, etc.)
 * @param crit Whether this damage was a critical hit (0=no, 1=yes)
 * @param raw Raw damage value before mitigation
 * @param mitig Amount of damage mitigated/absorbed
 * @param overkill Amount of damage that exceeded target's health (overkill)
 * @param execution Whether this damage resulted in target execution (0=no, 1=yes)
 *
 * @note The ring buffer automatically wraps around when capacity is reached
 * @note Windows error dialog suppression is installed early for test stability
 */
void rogue_damage_event_record_base(unsigned short attack_id, unsigned char dmg_type,
                                    unsigned char crit, int raw, int mitig, int overkill,
                                    unsigned char execution)
{
#if defined(_WIN32) && defined(ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS)
    (void) rogue_win_disable_error_dialogs_anchor; /* force link */
    rogue_win_disable_error_dialogs_install_if_needed();
#endif
    RogueDamageEvent* ev = &g_damage_events[g_damage_event_head % ROGUE_DAMAGE_EVENT_CAP];
    ev->attack_id = attack_id;
    ev->damage_type = dmg_type;
    ev->crit = crit;
    ev->raw_damage = raw;
    ev->mitigated = mitig;
    ev->overkill = overkill;
    ev->execution = execution ? 1 : 0;
    g_damage_event_head = (g_damage_event_head + 1) % ROGUE_DAMAGE_EVENT_CAP;
    g_damage_event_total++;
}
/**
 * @brief Public entry point for recording damage events
 *
 * Public interface for recording damage events. When the combat observer feature is not
 * compiled in, this function directly calls the base implementation. When observers are
 * enabled, the observer module provides its own implementation that wraps this base function.
 *
 * @param attack_id Unique identifier for the attack that caused this damage
 * @param dmg_type Type of damage (physical, magical, etc.)
 * @param crit Whether this damage was a critical hit (0=no, 1=yes)
 * @param raw Raw damage value before mitigation
 * @param mitig Amount of damage mitigated/absorbed
 * @param overkill Amount of damage that exceeded target's health (overkill)
 * @param execution Whether this damage resulted in target execution (0=no, 1=yes)
 *
 * @note This function is conditionally compiled - when ROGUE_FEATURE_COMBAT_OBSERVER is defined,
 *       the observer module provides the implementation instead
 */
#ifndef ROGUE_FEATURE_COMBAT_OBSERVER
void rogue_damage_event_record(unsigned short attack_id, unsigned char dmg_type, unsigned char crit,
                               int raw, int mitig, int overkill, unsigned char execution)
{
    rogue_damage_event_record_base(attack_id, dmg_type, crit, raw, mitig, overkill, execution);
}
#endif

/**
 * @brief Create a snapshot of recent damage events from the ring buffer
 *
 * Copies the most recent damage events from the ring buffer into the provided output array.
 * The function handles wraparound correctly and returns events in chronological order
 * (oldest to newest).
 *
 * @param out Output array to store the damage events (must not be NULL)
 * @param max_events Maximum number of events to copy (must be > 0)
 * @return Number of events actually copied (may be less than max_events if fewer events exist)
 *
 * @note If max_events exceeds available events, only available events are copied
 * @note Events are returned in chronological order (oldest first)
 * @note The ring buffer preserves the most recent events when capacity is exceeded
 */
int rogue_damage_events_snapshot(RogueDamageEvent* out, int max_events)
{
    if (!out || max_events <= 0)
        return 0;
    int count = (g_damage_event_total < ROGUE_DAMAGE_EVENT_CAP) ? g_damage_event_total
                                                                : ROGUE_DAMAGE_EVENT_CAP;
    if (count > max_events)
        count = max_events;
    int start = (g_damage_event_head - count);
    while (start < 0)
        start += ROGUE_DAMAGE_EVENT_CAP;
    for (int i = 0; i < count; i++)
        out[i] = g_damage_events[(start + i) % ROGUE_DAMAGE_EVENT_CAP];
    return count;
}

/**
 * @brief Clear all damage events from the ring buffer
 *
 * Resets the entire damage event ring buffer to its initial state, clearing all stored
 * events and resetting the head position and total count. All event fields are explicitly
 * zeroed for consistency.
 *
 * @note This function clears the entire buffer regardless of current contents
 * @note All event fields (attack_id, damage_type, crit, etc.) are explicitly zeroed
 * @note Buffer head and total counters are reset to 0
 */
void rogue_damage_events_clear(void)
{
    for (int i = 0; i < ROGUE_DAMAGE_EVENT_CAP; i++)
    {
        g_damage_events[i].attack_id = 0;
        g_damage_events[i].damage_type = 0;
        g_damage_events[i].crit = 0;
        g_damage_events[i].raw_damage = 0;
        g_damage_events[i].mitigated = 0;
        g_damage_events[i].overkill = 0;
        g_damage_events[i].execution = 0;
    }
    g_damage_event_head = 0;
    g_damage_event_total = 0;
}
