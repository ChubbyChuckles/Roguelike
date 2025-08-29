#define SDL_MAIN_HANDLED
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Stubs required by combat */
RoguePlayer g_exposed_player_for_stats = {0};
static int rogue_get_current_attack_frame(void) { return 3; }
static void rogue_app_add_hitstop(float ms) { (void) ms; }
static void rogue_add_damage_number(float x, float y, int amount, int from_player)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
}
static void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
    (void) crit;
}

int main()
{
    RogueEnemy e;
    memset(&e, 0, sizeof e);
    e.alive = 1;
    e.health = 100000;
    e.max_health = 100000;
    /* Case A: moderate defenses below soft cap threshold */
    e.armor = 50;
    e.resist_physical = 20;
    int over = 0;
    int raw = 400;
    int dmgA = rogue_apply_mitigation_enemy(&e, raw, ROGUE_DMG_PHYSICAL, &over);
    assert(dmgA > 0);
    /* Case B: very high defenses triggering soft cap. Use much larger armor + resist. */
    e.armor = 600;
    e.resist_physical = 80;
    int dmgB = rogue_apply_mitigation_enemy(&e, raw, ROGUE_DMG_PHYSICAL, &over);
    assert(dmgB > 0);
    printf("def_softcap: raw=%d below=%d above=%d\n", raw, dmgA, dmgB);
    /* Soft cap should prevent damage from dropping proportionally as much: ensure dmgB is not less
     * than 5% of raw (cap ~85%). */
    assert(dmgB >= (raw * 0.05f) - 1);
    /* And higher defenses still reduce more than moderate: dmgB <= dmgA */
    assert(dmgB <= dmgA);
    printf("phase3_def_softcap_basic: OK\n");
    return 0;
}
