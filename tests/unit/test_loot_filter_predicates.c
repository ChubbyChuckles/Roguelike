#include "core/loot/loot_filter.h"
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_instances.h"
#include "core/path_utils.h"
#include <stdio.h>
#include <string.h>

static int fail(const char* m){ fprintf(stderr,"PRED_FAIL:%s\n", m); return 1; }

/* Helper to write a temp filter file */
static int write_rules(const char* name, const char* text){ FILE* f=NULL; f=fopen(name,"wb"); if(!f) return 0; fputs(text,f); fclose(f); return 1; }

int main(void){
    /* Load broad item defs directory (weapons + materials) */
    char pitems[256]; if(!rogue_find_asset_path("items/swords.cfg", pitems, sizeof pitems)) return fail("path_items");
    for(int i=(int)strlen(pitems)-1;i>=0;i--){ if(pitems[i]=='/'||pitems[i]=='\\'){ pitems[i]='\0'; break; } }
    rogue_item_defs_reset(); if(rogue_item_defs_load_directory(pitems)<=0) return fail("load_defs");
    rogue_items_init_runtime();
    int iron = rogue_item_def_index("iron_sword");
    int steel = rogue_item_def_index("steel_sword");
    int epic = rogue_item_def_index("epic_blade");
    int mat = rogue_item_def_index("arcane_dust");
    if(iron<0||steel<0||epic<0||mat<0) return fail("defs_missing");
    int inst_iron = rogue_items_spawn(iron,1,1,1);
    int inst_steel = rogue_items_spawn(steel,1,2,2);
    int inst_epic  = rogue_items_spawn(epic,1,3,3);
    int inst_mat   = rogue_items_spawn(mat,5,4,4);
    if(inst_iron<0||inst_steel<0||inst_epic<0||inst_mat<0) return fail("spawn");

    /* Test 1: MODE=ALL rarity>=1 AND category=weapon => hides material only (iron rarity0 <1); steel (1) & epic (3) remain */
    if(!write_rules("flt.cfg","MODE=ALL\nrarity>=1\ncategory=weapon\n")) return fail("write1");
    rogue_loot_filter_reset(); if(rogue_loot_filter_load("flt.cfg")!=2) return fail("load1_count");
    rogue_loot_filter_refresh_instances();
    int vis = rogue_items_visible_count(); if(vis!=2) return fail("vis_all_expected2");

    /* Test 2: MODE=ANY def=iron_sword def=arcane_dust => only those two visible */
    if(!write_rules("flt.cfg","MODE=ANY\ndef=iron_sword\ndef=arcane_dust\n")) return fail("write2");
    rogue_loot_filter_reset(); if(rogue_loot_filter_load("flt.cfg")!=2) return fail("load2_count");
    rogue_loot_filter_refresh_instances();
    vis = rogue_items_visible_count(); if(vis!=2) return fail("vis_any_expected2");

    /* Test 3: MODE=ANY name~sword rarity>=3 => epic_blade passes (rarity>=3) and both swords pass name substring => visible=3 (iron, steel, epic) */
    if(!write_rules("flt.cfg","MODE=ANY\nname~sword\nrarity>=3\n")) return fail("write3");
    rogue_loot_filter_reset(); if(rogue_loot_filter_load("flt.cfg")!=2) return fail("load3_count");
    rogue_loot_filter_refresh_instances();
    vis = rogue_items_visible_count(); if(vis!=3) return fail("vis_any_expected3");

    printf("LOOT_FILTER_PRED_OK\n");
    return 0; }
