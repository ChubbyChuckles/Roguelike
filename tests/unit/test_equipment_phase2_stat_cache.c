/* Phase 2.1/2.2/2.5 Stat Cache Layering & Fingerprint Tests */
#include "../../src/core/app/app_state.h"
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/game/stat_cache.h"
#include "../../src/util/path_utils.h"
#include <assert.h>
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static void load_items(void)
{
    char p[256];
    if (rogue_find_asset_path("test_items.cfg", p, sizeof p))
    {
        rogue_item_defs_load_from_cfg(p);
    }
}

static void test_basic_layer_integrity(void)
{
    RoguePlayer player = {0};
    player.strength = 10;
    player.dexterity = 12;
    player.vitality = 20;
    player.intelligence = 5;
    player.max_health = 100;
    player.crit_chance = 5;
    player.crit_damage = 150;
    rogue_stat_cache_mark_dirty();
    rogue_stat_cache_force_update(&player);
    assert(g_player_stat_cache.base_strength == 10 && g_player_stat_cache.total_strength == 10);
    assert(g_player_stat_cache.base_dexterity == 12 && g_player_stat_cache.total_dexterity == 12);
    assert(g_player_stat_cache.dps_estimate > 0);
    unsigned long long fp1 = rogue_stat_cache_fingerprint();
    /* Change player stat -> dirty flag then update should change fingerprint */
    player.strength = 11;
    rogue_stat_cache_mark_dirty();
    rogue_stat_cache_update(&player);
    unsigned long long fp2 = rogue_stat_cache_fingerprint();
    assert(fp1 != fp2);
}

static void test_soft_cap_curve(void)
{
    float cap = 100.f, soft = 0.5f;
    float below = rogue_soft_cap_apply(80.f, cap, soft);
    float over = rogue_soft_cap_apply(200.f, cap, soft);
    assert(below == 80.f);
    assert(over < 200.f && over > 100.f);
}

static void test_soft_cap_monotonic_slope(void)
{
    /* Verify diminishing returns: incremental gain beyond cap shrinks with larger raw value. */
    float cap = 100.f, soft = 0.75f;
    float base = cap;
    float v1 = rogue_soft_cap_apply(base + 10.f, cap, soft);
    float v2 = rogue_soft_cap_apply(base + 20.f, cap, soft);
    float v3 = rogue_soft_cap_apply(base + 40.f, cap, soft);
    float d1 = v1 - cap;
    float d2 = v2 - v1;
    float d3 = v3 - v2;
    /* Each successive delta after the cap should not increase (d3 <= d2 <= d1). */
    assert(d1 > 0.f && d2 > 0.f && d3 > 0.f);
    assert(d2 <= d1 + 1e-4f);
    assert(d3 <= d2 + 1e-4f);
}

int main(void)
{
    rogue_item_defs_reset();
    load_items();
    rogue_equip_reset();
    test_basic_layer_integrity();
    test_soft_cap_curve();
    test_soft_cap_monotonic_slope();
    printf("stat_cache_phase2_ok\n");
    return 0;
}
