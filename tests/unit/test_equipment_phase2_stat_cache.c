/* Phase 2.1/2.2/2.5 Stat Cache Layering & Fingerprint Tests */
#include <assert.h>
#include <stdio.h>
#include "core/stat_cache.h"
#include "core/equipment.h"
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/app_state.h"
#include "core/path_utils.h"

RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

static void load_items(void){ char p[256]; if(rogue_find_asset_path("test_items.cfg", p,sizeof p)){ rogue_item_defs_load_from_cfg(p); } }

static void test_basic_layer_integrity(void){
    RoguePlayer player={0}; player.strength=10; player.dexterity=12; player.vitality=20; player.intelligence=5; player.max_health=100; player.crit_chance=5; player.crit_damage=150;
    rogue_stat_cache_mark_dirty(); rogue_stat_cache_force_update(&player);
    assert(g_player_stat_cache.base_strength==10 && g_player_stat_cache.total_strength==10);
    assert(g_player_stat_cache.base_dexterity==12 && g_player_stat_cache.total_dexterity==12);
    assert(g_player_stat_cache.dps_estimate>0);
    unsigned long long fp1 = rogue_stat_cache_fingerprint();
    /* Change player stat -> dirty flag then update should change fingerprint */
    player.strength=11; rogue_stat_cache_mark_dirty(); rogue_stat_cache_update(&player); unsigned long long fp2=rogue_stat_cache_fingerprint();
    assert(fp1!=fp2);
}

static void test_soft_cap_curve(void){
    float cap=100.f, soft=0.5f; float below = rogue_soft_cap_apply(80.f,cap,soft); float over = rogue_soft_cap_apply(200.f,cap,soft); assert(below==80.f); assert(over < 200.f && over > 100.f); }

int main(void){
    rogue_item_defs_reset(); load_items(); rogue_equip_reset();
    test_basic_layer_integrity();
    test_soft_cap_curve();
    printf("stat_cache_phase2_ok\n");
    return 0;
}
