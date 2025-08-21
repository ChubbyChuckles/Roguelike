#include "game/combat.h"
#if defined(_WIN32) && defined(ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS)
/* Pull in the suppression TU from the static lib and install early for tests. */
extern int rogue_win_disable_error_dialogs_anchor;
extern void rogue_win_disable_error_dialogs_install_if_needed(void);
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
/*
 * combat_events.c
 *   Isolated damage event ring + crit layering mode.
 *   Separation keeps frequently modified strike logic out of hot include path for faster
 * incremental builds.
 */
#if defined(_MSC_VER)
#define STATIC_ASSERT(expr, msg) typedef char static_assert_##msg[(expr) ? 1 : -1]
#else
#define STATIC_ASSERT(expr, msg) _Static_assert(expr, #msg)
#endif
STATIC_ASSERT(ROGUE_DAMAGE_EVENT_CAP <= 256, damage_event_cap_reasonable);

/* Damage event ring buffer (Phase 2.7) */
RogueDamageEvent g_damage_events[ROGUE_DAMAGE_EVENT_CAP];
int g_damage_event_head = 0;
int g_damage_event_total = 0;
int g_crit_layering_mode = 0; /* 0=pre-mitigation (legacy), 1=post-mitigation */
int g_force_crit_mode = -1;   /* -1 = RNG, 0 = force non-crit, 1 = force crit (tests) */

/* Base implementation separated so observer module can wrap */
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
/* Public entry point; if observer feature compiled, its object supplies the symbol (avoid
 * duplicate). */
#ifndef ROGUE_FEATURE_COMBAT_OBSERVER
void rogue_damage_event_record(unsigned short attack_id, unsigned char dmg_type, unsigned char crit,
                               int raw, int mitig, int overkill, unsigned char execution)
{
    rogue_damage_event_record_base(attack_id, dmg_type, crit, raw, mitig, overkill, execution);
}
#endif

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
