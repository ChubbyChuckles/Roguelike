#include <stdio.h>
#include <assert.h>
#include "entities/player.h"
#include "core/equipment.h"
#include "core/equipment_stats.h"
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include "core/path_utils.h"

/* Minimal deterministic test: create one weapon instance with agility affix and ensure dex increases after apply. */
int main(){
    rogue_item_defs_reset();
    char pitems[256]; if(!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems)){ printf("EQUIP_STAT_FAIL find_items\n"); return 1; }
    int added = rogue_item_defs_load_from_cfg(pitems);
    if(added<=0){ printf("EQUIP_STAT_FAIL item_defs count=%d\n", added); return 1; }
    rogue_affixes_reset();
    char paff[256]; if(!rogue_find_asset_path("affixes.cfg", paff, sizeof paff)){ printf("EQUIP_STAT_FAIL find_affixes\n"); return 2; }
    if(rogue_affixes_load_from_cfg(paff)<=0){ printf("EQUIP_STAT_FAIL affixes\n"); return 2; }
    rogue_items_init_runtime();
    int def_index = rogue_item_def_index("long_sword"); assert(def_index>=0);
    int inst = rogue_items_spawn(def_index,1,0,0); assert(inst>=0);
    RogueItemInstance* it = (RogueItemInstance*)rogue_item_instance_at(inst); assert(it);
    int agility_affix=-1; for(int i=0;i<rogue_affix_count();i++){ const RogueAffixDef* a = rogue_affix_at(i); if(a && a->stat==ROGUE_AFFIX_STAT_AGILITY_FLAT){ agility_affix=i; break; } }
    if(agility_affix<0){ printf("EQUIP_STAT_FAIL no_agility_affix\n"); return 3; }
    it->suffix_index = agility_affix; it->suffix_value = 3; /* deterministic value */
    rogue_equip_reset();
    int rc = rogue_equip_try(ROGUE_EQUIP_WEAPON, inst); if(rc!=0){ printf("EQUIP_STAT_FAIL equip rc=%d\n", rc); return 4; }
    RoguePlayer p; rogue_player_init(&p); int base_dex = p.dexterity; if(base_dex<=0){ printf("EQUIP_STAT_FAIL base_dex=%d\n", base_dex); return 5; }
    rogue_equipment_apply_stat_bonuses(&p);
    if(!(p.dexterity == base_dex + 3)){ printf("EQUIP_STAT_FAIL dex=%d expected=%d\n", p.dexterity, base_dex+3); return 6; }
    printf("EQUIP_STAT_OK base=%d final=%d\n", base_dex, p.dexterity);
    return 0;
}
