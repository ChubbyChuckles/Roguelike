#ifndef ROGUE_EQUIPMENT_GEMS_H
#define ROGUE_EQUIPMENT_GEMS_H

#include <stdint.h>

/* Phase 5.2 Gem definitions: Supports flat primary/resist/armor bonuses plus percent primary
 * scaling and metadata for conditional/proc chance. */
typedef struct RogueGemDef
{
    char id[32];
    int item_def_index; /* underlying item definition (category ROGUE_ITEM_GEM) */
    /* Flat contributions */
    int strength, dexterity, vitality, intelligence;
    int armor_flat;
    int resist_physical, resist_fire, resist_cold, resist_lightning, resist_poison, resist_status;
    /* Percent primary stat bonuses (applied multiplicatively after flat stacking) */
    int pct_strength, pct_dexterity, pct_vitality, pct_intelligence; /* percent (0..n) */
    /* Conditional / proc metadata (stored only; effect engine Phase 6) */
    int conditional_flags; /* bitmask placeholder */
    int proc_chance;       /* percent chance (0..100) placeholder */
} RogueGemDef;

int rogue_gem_register(const RogueGemDef* def); /* returns index or <0 */
const RogueGemDef* rogue_gem_at(int index);
int rogue_gem_find_by_item_def(int item_def_index); /* -1 if not found */
int rogue_gem_count(void);

/* Load gem defs from simple CSV:
 * id,item_def_id,str,dex,vit,int,armor,res_phys,res_fire,res_cold,res_light,res_poison,res_status,pct_str,pct_dex,pct_vit,pct_int,proc_chance,conditional_flags
 */
int rogue_gem_defs_load_from_cfg(const char* path);

/* Cost heuristic (Phase 5.3): base 10 + (sum flat + sum pct) * 2 */
int rogue_gem_socket_cost(const RogueGemDef* g);

/* High-level socket operations with economy & safety. Return 0 on success; negative codes mirror
 * base socket API plus -6 insufficient funds. */
int rogue_item_instance_socket_insert_pay(int inst_index, int slot, int gem_def_index,
                                          int* out_cost);
int rogue_item_instance_socket_remove_refund(int inst_index, int slot, int return_to_inventory);

/* Aggregation hook (invoked inside equipment_stats) */
void rogue_gems_aggregate_equipped(void);

#endif
