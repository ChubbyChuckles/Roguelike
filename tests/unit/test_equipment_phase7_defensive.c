/* Phase 7 initial defensive layer tests: block chance/value basic behavior */
#define SDL_MAIN_HANDLED 1   /* prevent SDL from redefining main */
#include "core/stat_cache.h" /* use real stat cache definition */
#include "game/combat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
extern struct RogueStatCache g_player_stat_cache; /* defined in core */

/* Minimal stubs for player derived recalc */
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static void setup_player(RoguePlayer* p)
{
    *p = (RoguePlayer){0};
    p->health = 100;
    p->max_health = 100;
    p->poise = 50;
    p->poise_max = 50;
    p->guard_meter = 50;
    p->guard_meter_max = 50;
    p->perfect_guard_window_ms = 120.0f;
}

/* Inject defensive stats directly into cache (simulating aggregated gear) */
static void inject_block(int chance, int value)
{
    g_player_stat_cache.block_chance = chance;
    g_player_stat_cache.block_value = value;
    g_player_stat_cache.dirty = 1;
}

int main(void)
{
    RoguePlayer p;
    setup_player(&p);
    inject_block(100, 15); /* force block */
    int blocked = 0, perfect = 0;
    int dmg = rogue_player_apply_incoming_melee(&p, 40.0f, 0, -1, 10, &blocked, &perfect);
    assert(blocked == 1); /* should block */
    assert(dmg == 25);    /* 40 - 15 block value */
    inject_block(0, 0);   /* disable chance */
    blocked = perfect = 0;
    dmg = rogue_player_apply_incoming_melee(&p, 40.0f, 0, -1, 0, &blocked, &perfect);
    assert(blocked == 0);
    printf("equipment_phase7_defensive_basic_ok\n");
    return 0;
}
