/* Equipment Phase 4.2 unique item layer test
   Verifies: unique registration, aggregation into unique_* layer, fingerprint mutation.
*/
#include <assert.h>
#include <stdio.h>
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/equipment.h"
#include "core/equipment_stats.h"
#include "core/equipment_uniques.h"
#include "core/stat_cache.h"
#include "entities/player.h"

static int spawn(const char* id){ int idx=rogue_item_def_index(id); assert(idx>=0); int inst=rogue_items_spawn(idx,1,0,0); assert(inst>=0); return inst; }

int main(void){
    rogue_item_defs_reset();
    FILE* f=fopen("unique_tmp_items.cfg","wb"); assert(f);
    fputs("# base items with implicit zeros\n",f);
    fputs("unique_blade,Unique Blade,2,1,1,10,3,5,0,none,0,0,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0\n",f);
    fputs("plain_helm,Plain Helm,3,1,1,8,0,0,1,none,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0\n",f);
    fclose(f);
    assert(rogue_item_defs_load_from_cfg("unique_tmp_items.cfg")==2);
    rogue_items_init_runtime();
    rogue_equip_reset();
    RoguePlayer p={0}; p.strength=5; p.dexterity=5; p.vitality=5; p.intelligence=5; p.max_health=50; p.crit_chance=5; p.crit_damage=150;
    RogueUniqueDef u={0};
#if defined(_MSC_VER)
    strcpy_s(u.id, sizeof u.id, "blade_of_kings");
    strcpy_s(u.base_item_id, sizeof u.base_item_id, "unique_blade");
#else
    strncpy(u.id,"blade_of_kings",sizeof u.id-1);
    strncpy(u.base_item_id,"unique_blade",sizeof u.base_item_id-1);
#endif
    u.strength=7; u.dexterity=0; u.vitality=2; u.intelligence=1; u.armor_flat=3; u.resist_fire=5; u.resist_cold=4;
    assert(rogue_unique_register(&u)>=0);

    int blade = spawn("unique_blade");
    assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, blade)==0);
    rogue_equipment_apply_stat_bonuses(&p); rogue_stat_cache_force_update(&p);
    assert(g_player_stat_cache.unique_strength==7);
    assert(g_player_stat_cache.unique_vitality==2);
    assert(g_player_stat_cache.resist_fire>=5 && g_player_stat_cache.resist_cold>=4);
    unsigned long long fp1 = rogue_stat_cache_fingerprint();
    /* Equip non-unique item (no change to unique layer) */
    int helm = spawn("plain_helm"); assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, helm)==0);
    rogue_equipment_apply_stat_bonuses(&p); rogue_stat_cache_force_update(&p);
    unsigned long long fp2 = rogue_stat_cache_fingerprint();
    assert(fp2!=0 && fp1!=0);
    /* Unique layer unchanged by plain helm; fingerprint may change due to armor/resist but unique stats remain */
    assert(g_player_stat_cache.unique_strength==7);

    /* Register second unique for helm and ensure mutation */
    RogueUniqueDef u2={0};
#if defined(_MSC_VER)
    strcpy_s(u2.id,sizeof u2.id,"crown_of_wisdom"); strcpy_s(u2.base_item_id,sizeof u2.base_item_id,"plain_helm");
#else
    strncpy(u2.id,"crown_of_wisdom",sizeof u2.id-1); strncpy(u2.base_item_id,"plain_helm",sizeof u2.base_item_id-1);
#endif
    u2.intelligence=6; u2.resist_lightning=8;
    assert(rogue_unique_register(&u2)>=0);
    rogue_equipment_apply_stat_bonuses(&p); rogue_stat_cache_force_update(&p);
    assert(g_player_stat_cache.unique_intelligence>=6);
    unsigned long long fp3 = rogue_stat_cache_fingerprint();
    assert(fp3 != fp2);
    printf("OK\n");
    return 0;
}
