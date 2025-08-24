/* Equipment Phase 4.1 implicit modifier layer test
   Verifies: parsing extended implicit columns, aggregation into implicit_* layer, fingerprint
   mutation, equip ordering invariance.
*/
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/equipment/equipment_stats.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/entities/player.h"
#include "../../src/game/stat_cache.h"
#include <assert.h>
#include <stdio.h>

static unsigned int rng_state = 123456789u;
static unsigned int lcg(unsigned int* s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

static int spawn_item(const char* id)
{
    int idx = rogue_item_def_index(id);
    assert(idx >= 0);
    int inst = rogue_items_spawn(idx, 1, 0, 0);
    assert(inst >= 0);
    return inst;
}

int main(void)
{
    rogue_item_defs_reset();
    /* Create a temporary cfg inline to exercise extended columns. */
    const char* path = "implicits_tmp.cfg";
    FILE* f = NULL;
    f = fopen(path, "wb");
    assert(f);
    /* header comment */
    fputs("# "
          "id,name,category,level_req,stack_max,base_value,dmg_min,dmg_max,armor,sheet,tx,ty,tw,th,"
          "rarity,flags,imp_str,imp_dex,imp_vit,imp_int,imp_armor,imp_rphys,imp_rfire,imp_rcold,"
          "imp_rlight,imp_rpoison,imp_rstatus,set_id\n",
          f);
    /* Two armor pieces with different implicit primaries/resists */
    fputs("helm_of_fortitude,Helm Fort,3,1,1,10,0,0,2,none,0,0,1,1,1,0,5,0,3,0,1,2,0,0,0,0,0\n", f);
    fputs("boots_of_flames,Flame Boots,3,1,1,12,0,0,1,none,0,0,1,1,1,0,0,4,0,0,0,0,6,0,0,0,0\n", f);
    fclose(f);
    int added = rogue_item_defs_load_from_cfg(path);
    assert(added == 2);
    rogue_items_init_runtime();
    rogue_equip_reset();
    RoguePlayer p = {0};
    p.strength = 10;
    p.dexterity = 5;
    p.vitality = 7;
    p.intelligence = 3;
    p.crit_chance = 10;
    p.crit_damage = 150;
    p.max_health = 100;
    int helm = spawn_item("helm_of_fortitude");
    int boots = spawn_item("boots_of_flames");
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, helm) == 0);
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_FEET, boots) == 0);
    rogue_equipment_apply_stat_bonuses(&p);
    rogue_stat_cache_force_update(&p);
    /* Implicit stat layer populated */
    assert(g_player_stat_cache.implicit_strength == 5);
    assert(g_player_stat_cache.implicit_dexterity == 4);
    assert(g_player_stat_cache.implicit_vitality == 0);
    assert(g_player_stat_cache.implicit_intelligence == 3);
    /* Resist contributions: helm physical=2, boots fire=6 */
    assert(g_player_stat_cache.resist_physical >= 2);
    assert(g_player_stat_cache.resist_fire >= 6);
    unsigned long long fp1 = rogue_stat_cache_fingerprint();
    /* Swap equip order (unequip/equip) and ensure fingerprint unchanged */
    rogue_equip_unequip(ROGUE_EQUIP_ARMOR_HEAD);
    rogue_equip_unequip(ROGUE_EQUIP_ARMOR_FEET);
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_FEET, boots) == 0);
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, helm) == 0);
    rogue_equipment_apply_stat_bonuses(&p);
    rogue_stat_cache_force_update(&p);
    unsigned long long fp2 = rogue_stat_cache_fingerprint();
    assert(fp1 == fp2);
    /* Modify implicit by adding a new item with non-zero stat; ensure fingerprint changes */
    f = fopen(path, "ab");
    assert(f);
    fputs("ring_of_power,Ring Pow,3,1,1,5,0,0,0,none,0,0,1,1,1,0,10,0,0,0,0,0,0,0,0,0,0\n", f);
    fclose(f);
    int added2 = rogue_item_defs_load_from_cfg(path);
    assert(added2 == 1);
    int ring = spawn_item("ring_of_power");
    assert(rogue_equip_try(ROGUE_EQUIP_RING1, ring) == 0);
    rogue_equipment_apply_stat_bonuses(&p);
    rogue_stat_cache_force_update(&p);
    unsigned long long fp3 = rogue_stat_cache_fingerprint();
    assert(fp3 != fp2);
    rogue_items_shutdown_runtime();
    printf("OK\n");
    return 0;
}
