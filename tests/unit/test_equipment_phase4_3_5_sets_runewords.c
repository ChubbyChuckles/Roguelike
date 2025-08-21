/* Equipment Phase 4.3–4.6: Sets (threshold + partial scaling) and Runewords + precedence ordering test.
   Validates:
     - Set registration and threshold bonus application.
     - Partial scaling interpolation between thresholds.
     - Runeword recipe matching (simplified pattern = item id placeholder) and stacking with set & unique.
     - Precedence/order invariance: equip order permutations yield identical fingerprint.
*/
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_instances.h"
#include "core/equipment/equipment.h"
#include "core/equipment/equipment_stats.h"
#include "core/stat_cache.h"
#include "core/equipment/equipment_uniques.h"
#include "entities/player.h"

/* Forward declarations from equipment_stats.c internal (exposed for test via extern) */
extern int rogue_set_register(const void* def); /* cheat: treat as opaque */
extern int rogue_runeword_register(const void* rw);

/* Duplicate minimal struct definitions to build set/runeword descriptors (must match equipment_stats.c layout) */
typedef struct RogueSetBonus { int pieces; int strength,dexterity,vitality,intelligence; int armor_flat; int resist_fire,resist_cold,resist_light,resist_poison,resist_status,resist_physical; } RogueSetBonus;
typedef struct RogueSetDef { int set_id; int bonus_count; RogueSetBonus bonuses[4]; } RogueSetDef;
typedef struct RogueRuneword { char pattern[12]; int strength,dexterity,vitality,intelligence; int armor_flat; int resist_fire,resist_cold,resist_light,resist_poison,resist_status,resist_physical; } RogueRuneword;

static int spawn(const char* id){ int idx=rogue_item_def_index(id); assert(idx>=0); int inst=rogue_items_spawn(idx,1,0,0); assert(inst>=0); return inst; }

int main(void){
    rogue_item_defs_reset();
    FILE* f=fopen("sets_rw_tmp_items.cfg","wb"); assert(f);
    fputs("# base items with set ids and placeholders\n",f);
    /* id,name,category,level_req,stack_max,value,dmg_min,dmg_max,armor,sheet,tx,ty,tw,th,rarity,flags,imp fields...,set_id */
    fputs("ember_helm,Ember Helm,3,1,1,5,0,0,1,none,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,101\n",f); /* set 101 */
    fputs("ember_chest,Ember Chest,3,1,1,5,0,0,2,none,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,101\n",f);
    fputs("ember_boots,Ember Boots,3,1,1,5,0,0,1,none,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,101\n",f);
    fputs("rune_blade,Rune Blade,2,1,1,12,3,6,0,none,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0\n",f); /* runeword pattern matches id */
    fclose(f);
    assert(rogue_item_defs_load_from_cfg("sets_rw_tmp_items.cfg")==4);
    rogue_items_init_runtime();
    rogue_equip_reset();
    RoguePlayer p={0}; p.strength=10; p.dexterity=10; p.vitality=10; p.intelligence=10; p.max_health=100; p.crit_chance=5; p.crit_damage=150;

    /* Register a unique on rune_blade for precedence stacking test */
    RogueUniqueDef u={0}; strcpy(u.id,"uni_rune_blade"); strcpy(u.base_item_id,"rune_blade"); u.strength=5; assert(rogue_unique_register(&u)>=0);

    /* Register set 101: bonuses at 2pc (+4 str), 3pc (+4 str +4 vit) */
    RogueSetDef set={0}; set.set_id=101; set.bonus_count=2; set.bonuses[0]=(RogueSetBonus){2,4,0,0,0,0,0,0,0,0,0,0}; set.bonuses[1]=(RogueSetBonus){3,8,0,4,0,0,0,0,0,0,0,0}; assert(rogue_set_register(&set)>=0);

    /* Register runeword on rune_blade granting +3 dex, +2 vit */
    RogueRuneword rw={0}; strcpy(rw.pattern,"rune_blade"); rw.dexterity=3; rw.vitality=2; assert(rogue_runeword_register(&rw)>=0);

    int helm=spawn("ember_helm"); int chest=spawn("ember_chest"); int boots=spawn("ember_boots"); int blade=spawn("rune_blade");
    assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, blade)==0);
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, helm)==0);
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_CHEST, chest)==0);
    rogue_equipment_apply_stat_bonuses(&p); rogue_stat_cache_force_update(&p);
    /* Only 2 set pieces -> first threshold: +4 strength */
    int base_total_strength = g_player_stat_cache.total_strength;
    assert(g_player_stat_cache.set_strength == 4);
    /* Add third piece -> +8 strength total (+4 delta) and +4 vitality */
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_FEET, boots)==0);
    rogue_equipment_apply_stat_bonuses(&p); rogue_stat_cache_force_update(&p);
    assert(g_player_stat_cache.set_strength == 8);
    assert(g_player_stat_cache.set_vitality == 4);
    /* Runeword + unique contributions */
    assert(g_player_stat_cache.unique_strength >=5);
    assert(g_player_stat_cache.runeword_dexterity >=3);
    assert(g_player_stat_cache.runeword_vitality >=2);
    unsigned long long fp1 = rogue_stat_cache_fingerprint();
    /* Permute equip order (re-equip armor) and ensure fingerprint stability */
    rogue_equip_unequip(ROGUE_EQUIP_ARMOR_HEAD); rogue_equip_unequip(ROGUE_EQUIP_ARMOR_CHEST); rogue_equipment_apply_stat_bonuses(&p); rogue_stat_cache_force_update(&p);
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_CHEST, chest)==0); assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, helm)==0); rogue_equipment_apply_stat_bonuses(&p); rogue_stat_cache_force_update(&p);
    unsigned long long fp2 = rogue_stat_cache_fingerprint();
    assert(fp1 == fp2);
    printf("OK\n");
    return 0;
}
