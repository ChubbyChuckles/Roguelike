#include "../../src/game/combat.h"
#include <stdio.h>

static int g_observed = 0;
static RogueDamageEvent g_last;
static void on_damage(const RogueDamageEvent* ev, void* user)
{
    (void) user;
    g_observed++;
    g_last = *ev;
}

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    rogue_combat_clear_damage_observers();
    int id = rogue_combat_add_damage_observer(on_damage, NULL);
    if (id < 0)
    {
        printf("FAIL add observer\n");
        return 1;
    }
    rogue_damage_event_record(123, 1, 1, 50, 30, 0, 0);
    if (g_observed != 1)
    {
        printf("FAIL observer not invoked\n");
        return 1;
    }
    if (g_last.attack_id != 123 || g_last.raw_damage != 50 || !g_last.crit)
    {
        printf("FAIL event fields mismatch\n");
        return 1;
    }
    rogue_combat_remove_damage_observer(id);
    g_observed = 0;
    rogue_damage_event_record(124, 1, 0, 10, 5, 0, 0);
    if (g_observed != 0)
    {
        printf("FAIL observer still invoked after remove\n");
        return 1;
    }
    int id2 = rogue_combat_add_damage_observer(on_damage, NULL);
    if (id2 < 0)
    {
        printf("FAIL add2\n");
        return 1;
    }
    rogue_combat_clear_damage_observers();
    g_observed = 0;
    rogue_damage_event_record(200, 1, 0, 1, 1, 0, 0);
    if (g_observed != 0)
    {
        printf("FAIL clear observers\n");
        return 1;
    }
    printf("OK test_combat_damage_observer\n");
    return 0;
}
