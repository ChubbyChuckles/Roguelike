#include "core/crafting.h"
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include "core/inventory.h"
#include "core/economy.h"
#include "core/path_utils.h"
#include <stdio.h>
#include <string.h>

static int inv_get(int di){ return rogue_inventory_get_count(di); }
static int inv_add(int di,int q){ return rogue_inventory_add(di,q); }
static int inv_consume(int di,int q){ return rogue_inventory_consume(di,q); }
static int spend_gold(int amount){ if(amount<=0) return 0; if(rogue_econ_gold()<amount) return -1; rogue_econ_add_gold(-amount); return 0; }

static int reroll_affixes(int inst_index, unsigned int* rng_state, int rarity){ return rogue_item_instance_generate_affixes(inst_index, rng_state, rarity); }

int main(void){
    /* load affixes first */
    char paff[256]; if(!rogue_find_asset_path("affixes.cfg", paff, sizeof paff)){ fprintf(stderr,"REROLL_FAIL find affixes\n"); return 5; }
    rogue_affixes_reset(); if(rogue_affixes_load_from_cfg(paff) <= 0){ fprintf(stderr,"REROLL_FAIL load affixes\n"); return 6; }
    char pitems[256]; if(!rogue_find_asset_path("items/swords.cfg", pitems, sizeof pitems)){ fprintf(stderr,"REROLL_FAIL find swords\n"); return 10; }
    for(int i=(int)strlen(pitems)-1;i>=0;i--){ if(pitems[i]=='/'||pitems[i]=='\\'){ pitems[i]='\0'; break; } }
    rogue_item_defs_reset(); if(rogue_item_defs_load_directory(pitems)<=0){ fprintf(stderr,"REROLL_FAIL load dir\n"); return 11; }
    rogue_inventory_reset(); rogue_items_init_runtime(); rogue_econ_reset();
    int sword = rogue_item_def_index("iron_sword"); if(sword<0){ fprintf(stderr,"REROLL_FAIL iron_sword\n"); return 12; }
    unsigned int rng_state = 123u;
    int inst = rogue_items_spawn(sword,1,0,0); if(inst<0){ fprintf(stderr,"REROLL_FAIL spawn\n"); return 13; }
    /* Generate initial affixes so reroll has something to change */
    const RogueItemInstance* it0 = rogue_item_instance_at(inst); unsigned int seed0 = rng_state; rogue_item_instance_generate_affixes(inst, &seed0, 3);
    /* seed some materials + gold */
    int dust = rogue_item_def_index("arcane_dust"); if(dust<0){ fprintf(stderr,"REROLL_FAIL dust missing\n"); return 14; }
    rogue_inventory_add(dust,10); rogue_econ_add_gold(1000);
    const RogueItemInstance* it = rogue_item_instance_at(inst);
    int before_prefix = it->prefix_index;
    /* Clear prefix to ensure reroll assigns something potentially new */
    if(before_prefix>=0){ RogueItemInstance* mut = (RogueItemInstance*)it; mut->prefix_index = -1; }
    fprintf(stderr,"DEBUG before reroll rarity=%d before_pref=%d\n", it->rarity, before_prefix);
    int elevated_rarity = 3; /* ensure generation path that rolls at least one prefix */
    int rc = rogue_craft_reroll_affixes(inst, elevated_rarity, dust, 5, 100, inv_get, inv_consume, spend_gold, reroll_affixes, &rng_state);
    const RogueItemInstance* it_after_dbg = rogue_item_instance_at(inst);
    fprintf(stderr,"DEBUG after reroll rc=%d new_pref=%d\n", rc, it_after_dbg? it_after_dbg->prefix_index : -999);
    if(rc != 0){ fprintf(stderr,"REROLL_FAIL api rc=%d inst=%d rarity=%d before_pref=%d after_pref=%d\n", rc, inst, it->rarity, before_prefix, it_after_dbg? it_after_dbg->prefix_index : -999); return 15; }
    const RogueItemInstance* it2 = rogue_item_instance_at(inst);
    if(it2->prefix_index < 0){ fprintf(stderr,"REROLL_FAIL no prefix_after\n"); return 16; }
    if(rogue_inventory_get_count(dust) != 5){ fprintf(stderr,"REROLL_FAIL material not consumed\n"); return 17; }
    if(rogue_econ_gold() != 900){ fprintf(stderr,"REROLL_FAIL gold not spent\n"); return 18; }
    printf("REROLL_OK prefix_before=%d prefix_after=%d\n", before_prefix, it2->prefix_index);
    return 0;
}
