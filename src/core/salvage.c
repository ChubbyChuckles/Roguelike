#include "core/salvage.h"
#include "core/loot_item_defs.h"
#include <stdio.h>

/* Simple salvage rule: materials per rarity = 1,2,4,8,16 scaled by base_value bracket. Uses 'arcane_dust' for rarity <3, 'primal_shard' for rarity >=3 if present. */

static int find_material(const char* id){ return rogue_item_def_index(id); }

int rogue_salvage_item(int item_def_index, int rarity, int (*add_material_cb)(int,int)){
    const RogueItemDef* d = rogue_item_def_at(item_def_index); if(!d || !add_material_cb) return 0;
    int mult[5]={1,2,4,8,16}; if(rarity<0) rarity=0; if(rarity>4) rarity=4; int base = mult[rarity];
    /* Scale by value bracket (<=50 -> x1, <=150 -> x2, >150 -> x3) */
    int scale = (d->base_value>150)?3: (d->base_value>50?2:1);
    int qty = base * scale;
    const char* mat_id = (rarity>=3)? "primal_shard" : "arcane_dust";
    int mat_def = find_material(mat_id);
    if(mat_def<0){ fprintf(stderr,"salvage: missing material def %s\n", mat_id); return 0; }
    if(qty<1) qty=1; add_material_cb(mat_def, qty); return qty;
}
