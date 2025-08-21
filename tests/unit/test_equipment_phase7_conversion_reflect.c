#define SDL_MAIN_HANDLED 1
#include "core/stat_cache.h"
#include "game/combat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct RogueStatCache g_player_stat_cache; /* real cache */

/* Minimal stubs */
RoguePlayer g_exposed_player_for_stats; /* referenced by systems */
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static void setup_player(RoguePlayer* p)
{
    memset(p, 0, sizeof *p);
    p->health = 100;
    p->max_health = 100;
    p->poise = 40;
    p->poise_max = 40;
    p->guard_meter = 60;
    p->guard_meter_max = 60;
    p->perfect_guard_window_ms = 120.0f;
}

static void reset_cache() { memset(&g_player_stat_cache, 0, sizeof g_player_stat_cache); }

/* Deterministic RNG substitute for passive block rolls (force no block) */
int main(void)
{
    RoguePlayer p;
    setup_player(&p);
    reset_cache();
    /* Inject conversion: 30% fire, 20% frost, 10% arcane (total 60) */
    g_player_stat_cache.phys_conv_fire_pct = 30;
    g_player_stat_cache.phys_conv_frost_pct = 20;
    g_player_stat_cache.phys_conv_arcane_pct = 10; /* 60% total */
    /* Inject thorns: 25% reflect capped at 12 */
    g_player_stat_cache.thorns_percent = 25;
    g_player_stat_cache.thorns_cap = 12;
    /* Disable block chance to isolate conversion */
    g_player_stat_cache.block_chance = 0;
    g_player_stat_cache.block_value = 0;
    int blocked = 0, perfect = 0;
    int dmg = rogue_player_apply_incoming_melee(&p, 100.0f, 0, -1, 0, &blocked, &perfect);
    /* Damage should remain 100 (conversion conserves) */
    assert(dmg == 100);
    /* Now test conversion cap: push over 95% attempted */
    reset_cache();
    setup_player(&p);
    g_player_stat_cache.phys_conv_fire_pct = 70;
    g_player_stat_cache.phys_conv_frost_pct = 40;
    g_player_stat_cache.phys_conv_arcane_pct = 10; /* 120 => capped to 95 */
    dmg = rogue_player_apply_incoming_melee(&p, 200.0f, 0, -1, 0, &blocked, &perfect);
    assert(dmg == 200); /* still conserved */
    /* Guard recovery modifier scaling (Phase 7.3): simulate regen tick */
    reset_cache();
    setup_player(&p);
    p.guard_meter = 10.0f;
    p.guarding = 0;
    g_player_stat_cache.guard_recovery_pct = 50;       /* +50% regen */
    rogue_player_update_guard(&p, 100.0f);             /* 100 ms */
    float expected = 10.0f + (100.0f * 0.030f * 1.5f); /* base * (1+0.5) */
    assert(p.guard_meter > expected - 0.01f && p.guard_meter < expected + 0.01f);
    printf("equipment_phase7_conversion_reflect_ok\n");
    return 0;
}
