#include "core/equipment_enchant.h"
#include "core/loot_instances.h"
#include "core/loot_item_defs.h"
#include "core/loot_affixes.h"
#include "core/economy.h"
#include "core/inventory.h"
#include "core/stat_cache.h"
#include <stdlib.h>

/* Material IDs looked up lazily (cached after first resolution) */
static int g_enchant_mat_def = -1; /* enchant_orb */
static int g_reforge_mat_def = -1; /* reforge_hammer */

static void resolve_material_ids(void){
    if(g_enchant_mat_def<0) g_enchant_mat_def = rogue_item_def_index("enchant_orb");
    if(g_reforge_mat_def<0) g_reforge_mat_def = rogue_item_def_index("reforge_hammer");
}

static int enchant_cost_formula(int item_level, int rarity, int slots){
    if(item_level<1) item_level=1; if(rarity<0) rarity=0; if(rarity>4) rarity=4; if(slots<0) slots=0;
    /* Base 50 + item_level*5 + rarity^2 * 25 + 10*slots */
    return 50 + item_level*5 + (rarity*rarity)*25 + 10*slots;
}
static int reforge_cost_formula(int item_level, int rarity, int slots){
    return enchant_cost_formula(item_level,rarity,slots) * 2; /* Reforge is heavier */
}

static void reroll_affix(int *index_field, int *value_field, RogueAffixType type, int rarity, unsigned int* rng){
    if(!index_field || !value_field) return; *index_field = -1; *value_field = 0;
    int idx = rogue_affix_roll(type, rarity, rng); if(idx>=0){ *index_field = idx; *value_field = rogue_affix_roll_value(idx, rng); }
}

int rogue_item_instance_enchant(int inst_index, int reroll_prefix, int reroll_suffix, int* out_cost){
    const RogueItemInstance* itc = rogue_item_instance_at(inst_index); if(!itc) return -1; const RogueItemDef* d=rogue_item_def_at(itc->def_index); if(!d) return -1;
    if(!reroll_prefix && !reroll_suffix) return -2;
    int has_prefix = itc->prefix_index>=0; int has_suffix = itc->suffix_index>=0;
    if(reroll_prefix && !has_prefix && reroll_suffix && !has_suffix) return -2; /* neither present */
    unsigned int rng = (unsigned int)(inst_index*2654435761u) ^ (unsigned int)itc->item_level ^ 0xBEEF1234u; /* deterministic seed basis */
    int cost = enchant_cost_formula(itc->item_level, itc->rarity, itc->socket_count);
    resolve_material_ids();
    int need_mat = (reroll_prefix && reroll_suffix)?1:0;
    if(rogue_econ_gold() < cost) return -3;
    if(need_mat){ if(g_enchant_mat_def<0 || rogue_inventory_get_count(g_enchant_mat_def)<=0) return -4; }
    /* Deduct resources */
    rogue_econ_add_gold(-cost); if(need_mat) rogue_inventory_consume(g_enchant_mat_def,1);
    /* Copy to mutable */
    RogueItemInstance* it = (RogueItemInstance*)itc;
    if(reroll_prefix && has_prefix){ reroll_affix(&it->prefix_index,&it->prefix_value,ROGUE_AFFIX_PREFIX,it->rarity,&rng); }
    if(reroll_suffix && has_suffix){ reroll_affix(&it->suffix_index,&it->suffix_value,ROGUE_AFFIX_SUFFIX,it->rarity,&rng); }
    /* Budget validation */
    if(rogue_item_instance_validate_budget(inst_index)!=0) return -5; /* Should not happen; treat as failure */
    rogue_stat_cache_mark_dirty(); if(out_cost) *out_cost = cost; return 0;
}

int rogue_item_instance_reforge(int inst_index, int* out_cost){
    const RogueItemInstance* itc = rogue_item_instance_at(inst_index); if(!itc) return -1; const RogueItemDef* d=rogue_item_def_at(itc->def_index); if(!d) return -1;
    unsigned int rng = (unsigned int)(inst_index*11400714819323198485ull) ^ (unsigned int)itc->item_level ^ 0xC0FFEEU;
    resolve_material_ids();
    int cost = reforge_cost_formula(itc->item_level,itc->rarity,itc->socket_count);
    if(rogue_econ_gold() < cost) return -3;
    if(g_reforge_mat_def<0 || rogue_inventory_get_count(g_reforge_mat_def)<=0) return -6;
    /* Deduct */
    rogue_econ_add_gold(-cost); rogue_inventory_consume(g_reforge_mat_def,1);
    RogueItemInstance* it=(RogueItemInstance*)itc;
    /* Wipe affixes */
    it->prefix_index = it->suffix_index = -1; it->prefix_value = it->suffix_value = 0;
    /* Rarity determines how many affixes to roll similar to original generation (reuse logic) */
    int rarity = it->rarity;
    /* Basic mimic of generation rules */
    if(rarity>=2){ if(rarity>=3){ reroll_affix(&it->prefix_index,&it->prefix_value,ROGUE_AFFIX_PREFIX,rarity,&rng); reroll_affix(&it->suffix_index,&it->suffix_value,ROGUE_AFFIX_SUFFIX,rarity,&rng); }
        else { int choose_prefix = (rng & 1)==0; if(choose_prefix) reroll_affix(&it->prefix_index,&it->prefix_value,ROGUE_AFFIX_PREFIX,rarity,&rng); else reroll_affix(&it->suffix_index,&it->suffix_value,ROGUE_AFFIX_SUFFIX,rarity,&rng); }
    }
    /* Clear existing gem sockets content (preserve count) */
    for(int s=0;s<it->socket_count && s<6; ++s){ it->sockets[s] = -1; }
    if(rogue_item_instance_validate_budget(inst_index)!=0) return -5;
    rogue_stat_cache_mark_dirty(); if(out_cost) *out_cost=cost; return 0;
}
