#define SDL_MAIN_HANDLED 1
#include "../../src/core/equipment/equipment_procs.h"
#include "../../src/core/stat_cache.h"
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern struct RogueStatCache g_player_stat_cache;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static void setup_player(RoguePlayer* p)
{
    memset(p, 0, sizeof *p);
    p->health = 100;
    p->max_health = 100;
    p->poise = 40;
    p->poise_max = 40;
    p->guard_meter = 50;
    p->guard_meter_max = 50;
    p->perfect_guard_window_ms = 80.0f;
}

int main()
{
    RoguePlayer p;
    setup_player(&p);
    memset(&g_player_stat_cache, 0, sizeof g_player_stat_cache);
    /* Register a block-trigger reactive shield proc (magnitude=20 absorb per stack, refresh rule).
     */
    RogueProcDef shield = {0};
    shield.trigger = ROGUE_PROC_ON_BLOCK;
    shield.icd_ms = 0;
    shield.duration_ms = 1000;
    shield.magnitude = 20;
    shield.max_stacks = 0;
    shield.stack_rule = ROGUE_PROC_STACK_REFRESH;
    shield.param = 0;
    int id = rogue_proc_register(&shield);
    assert(id >= 0);
    /* Force activate (simulate prior block) with 1 stack */
    rogue_proc_force_activate(id, 1, 1000);
    int blocked = 0, perfect = 0;
    g_player_stat_cache.block_chance = 100;
    g_player_stat_cache.block_value = 0; /* force passive block for path */
    int dmg = rogue_player_apply_incoming_melee(&p, 25.0f, 0, -1, 0, &blocked, &perfect);
    /* Block triggers, block_value=0 so raw 25 enters conversion (none) then absorb consumes up to
     * 20 => 5 left */
    assert(dmg == 5);
    /* Shield should now be consumed (stack gone) */
    assert(rogue_procs_absorb_pool() == 0);
    printf("equipment_phase7_reactive_shield_ok\n");
    return 0;
}
