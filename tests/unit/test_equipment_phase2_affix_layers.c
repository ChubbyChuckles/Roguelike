/* Equipment Phase 2 layered affix aggregation & fingerprint determinism test */
#include <assert.h>
#include <stdio.h>
#include "core/stat_cache.h"
#include "core/equipment.h"
#include "core/loot_affixes.h"
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/equipment_stats.h"
#include "core/app_state.h"

RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){(void)p;}

static void seed_affixes(void){
    rogue_affixes_reset();
    /* type,id,stat,min,max,w0..w4 */
    /* Provide deterministic attribute and armor affixes */
    const char* path = "affix_tmp.cfg";
    FILE* f = fopen(path,"wb");
    fprintf(f,"PREFIX,str_flat,strength_flat,3,3,10,10,10,10,10\n");
    fprintf(f,"PREFIX,dex_flat,dexterity_flat,4,4,10,10,10,10,10\n");
    fprintf(f,"PREFIX,vit_flat,vitality_flat,5,5,10,10,10,10,10\n");
    fprintf(f,"PREFIX,int_flat,intelligence_flat,6,6,10,10,10,10,10\n");
    fprintf(f,"SUFFIX,armor_flat,armor_flat,7,7,10,10,10,10,10\n");
    fclose(f);
    rogue_affixes_load_from_cfg(path);
}

static void make_item_defs(void){
    rogue_item_defs_reset();
    const char* path = "item_tmp.cfg";
    FILE* f = fopen(path,"wb");
    /* id,name,category,level_req,stack_max,base_value,dmg_min,dmg_max,armor,sheet,tx,ty,tw,th,rarity */
    /* Ensure no stray spaces; parser expects at least 14 fields before rarity (15th). */
    fprintf(f,"blade_basic,BladeBasic,2,1,1,10,3,5,0,sheet.png,0,0,1,1,1\n");
    fprintf(f,"helm_basic,HelmBasic,3,1,1,8,0,0,2,sheet.png,0,0,1,1,1\n");
    fclose(f);
    rogue_item_defs_load_from_cfg(path);
}

/* Spawn an item instance from definition id then attach deterministic affixes (prefix/suffix). */
static int spawn_item(const char* id, const char* prefix, const char* suffix){
    int def = rogue_item_def_index(id); assert(def>=0);
    int inst_index = rogue_items_spawn(def, 1, 0.f, 0.f); assert(inst_index>=0);
    RogueItemInstance* it = (RogueItemInstance*)rogue_item_instance_at(inst_index); assert(it);
    if(prefix){ int ai = rogue_affix_index(prefix); assert(ai>=0); it->prefix_index = ai; it->prefix_value = rogue_affix_at(ai)->min_value; }
    if(suffix){ int ai = rogue_affix_index(suffix); assert(ai>=0); it->suffix_index = ai; it->suffix_value = rogue_affix_at(ai)->min_value; }
    return inst_index;
}

static void test_affix_layer_and_fingerprint(void){
    RoguePlayer player={0}; player.strength=10; player.dexterity=10; player.vitality=10; player.intelligence=10; player.max_health=100; player.crit_chance=5; player.crit_damage=150;
    int blade = spawn_item("blade_basic","str_flat","dex_flat");
    int helm = spawn_item("helm_basic","vit_flat","armor_flat");
    assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, blade)==0);
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, helm)==0);
    rogue_equipment_apply_stat_bonuses(&player); /* populates affix_* */
    rogue_stat_cache_mark_dirty();
    rogue_stat_cache_force_update(&player);
    assert(g_player_stat_cache.affix_strength==3);
    assert(g_player_stat_cache.affix_dexterity==4);
    assert(g_player_stat_cache.affix_vitality==5);
    assert(g_player_stat_cache.affix_armor_flat==7);
    unsigned long long fp1 = rogue_stat_cache_fingerprint();
    /* Add intelligence affix to weapon via suffix swap */
    int int_affix = rogue_affix_index("int_flat"); assert(int_affix>=0);
    RogueItemInstance* blade_inst = (RogueItemInstance*)rogue_item_instance_at(blade);
    blade_inst->suffix_index = int_affix; blade_inst->suffix_value = rogue_affix_at(int_affix)->min_value;
    rogue_equipment_apply_stat_bonuses(&player);
    rogue_stat_cache_mark_dirty();
    rogue_stat_cache_force_update(&player);
    assert(g_player_stat_cache.affix_intelligence==6);
    unsigned long long fp2 = rogue_stat_cache_fingerprint();
    assert(fp2!=fp1);
}

/* Equip ordering invariance: equipping items in different orders should yield identical layered totals & fingerprint. */
static void test_ordering_invariance(void){
    RoguePlayer player={0}; player.strength=5; player.dexterity=5; player.vitality=5; player.intelligence=5; player.max_health=50; player.crit_chance=5; player.crit_damage=150;
    rogue_equip_reset();
    int helm = spawn_item("helm_basic","vit_flat","armor_flat");
    int blade = spawn_item("blade_basic","str_flat","dex_flat");
    /* Order A */
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, helm)==0);
    assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, blade)==0);
    rogue_equipment_apply_stat_bonuses(&player); rogue_stat_cache_mark_dirty(); rogue_stat_cache_force_update(&player);
    unsigned long long fpA = rogue_stat_cache_fingerprint();
    int total_str_A = g_player_stat_cache.total_strength;
    int total_dex_A = g_player_stat_cache.total_dexterity;
    int total_vit_A = g_player_stat_cache.total_vitality;
    /* Order B (reset + reverse) */
    rogue_equip_reset(); g_player_stat_cache.affix_strength=g_player_stat_cache.affix_dexterity=g_player_stat_cache.affix_vitality=g_player_stat_cache.affix_intelligence=0; g_player_stat_cache.affix_armor_flat=0;
    assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, blade)==0);
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, helm)==0);
    rogue_equipment_apply_stat_bonuses(&player); rogue_stat_cache_mark_dirty(); rogue_stat_cache_force_update(&player);
    unsigned long long fpB = rogue_stat_cache_fingerprint();
    assert(fpA==fpB);
    assert(total_str_A==g_player_stat_cache.total_strength);
    assert(total_dex_A==g_player_stat_cache.total_dexterity);
    assert(total_vit_A==g_player_stat_cache.total_vitality);
}

int main(void){
    seed_affixes();
    make_item_defs();
    test_affix_layer_and_fingerprint();
    test_ordering_invariance();
    printf("phase2_affix_layers_ok\n");
    return 0;
}
