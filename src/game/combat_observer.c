#include "game/combat.h"

#define ROGUE_MAX_DAMAGE_OBSERVERS 16
typedef struct
{
    RogueDamageObserverFn fn;
    void* user;
} RogueDamageObserverSlot;
static RogueDamageObserverSlot g_damage_observers[ROGUE_MAX_DAMAGE_OBSERVERS];

int rogue_combat_add_damage_observer(RogueDamageObserverFn fn, void* user)
{
    if (!fn)
        return -1;
    for (int i = 0; i < ROGUE_MAX_DAMAGE_OBSERVERS; i++)
        if (!g_damage_observers[i].fn)
        {
            g_damage_observers[i].fn = fn;
            g_damage_observers[i].user = user;
            return i;
        }
    return -1;
}
void rogue_combat_remove_damage_observer(int id)
{
    if (id < 0 || id >= ROGUE_MAX_DAMAGE_OBSERVERS)
        return;
    g_damage_observers[id].fn = NULL;
    g_damage_observers[id].user = NULL;
}
void rogue_combat_clear_damage_observers(void)
{
    for (int i = 0; i < ROGUE_MAX_DAMAGE_OBSERVERS; i++)
    {
        g_damage_observers[i].fn = NULL;
        g_damage_observers[i].user = NULL;
    }
}

extern void rogue_damage_event_record_base(unsigned short attack_id, unsigned char dmg_type,
                                           unsigned char crit, int raw, int mitig, int overkill,
                                           unsigned char execution);
extern int g_damage_event_head;
extern RogueDamageEvent g_damage_events[];

/* Override public entry to extend with observer dispatch */
void rogue_damage_event_record(unsigned short attack_id, unsigned char dmg_type, unsigned char crit,
                               int raw, int mitig, int overkill, unsigned char execution)
{
    rogue_damage_event_record_base(attack_id, dmg_type, crit, raw, mitig, overkill, execution);
    int idx = (g_damage_event_head - 1);
    while (idx < 0)
        idx += ROGUE_DAMAGE_EVENT_CAP;
    idx %= ROGUE_DAMAGE_EVENT_CAP;
    const RogueDamageEvent* ev = &g_damage_events[idx];
    for (int i = 0; i < ROGUE_MAX_DAMAGE_OBSERVERS; i++)
        if (g_damage_observers[i].fn)
            g_damage_observers[i].fn(ev, g_damage_observers[i].user);
}
