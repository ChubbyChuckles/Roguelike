#include "core/equipment/equipment.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/stat_cache.h"
#include "core/equipment/equipment_stats.h" /* ensure rogue_equipment_apply_stat_bonuses prototype */
#include "core/vendor/economy.h"
#include <string.h>

static int g_slots[ROGUE_EQUIP__COUNT];
static int g_transmog_defs[ROGUE_EQUIP__COUNT]; /* -1 = none */

void rogue_equip_reset(void){ for(int i=0;i<ROGUE_EQUIP__COUNT;i++){ g_slots[i]=-1; g_transmog_defs[i]=-1; } }
int rogue_equip_get(enum RogueEquipSlot slot){ if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; return g_slots[slot]; }

static int category_for_slot(enum RogueEquipSlot slot){
	switch(slot){
		case ROGUE_EQUIP_WEAPON: return ROGUE_ITEM_WEAPON;
		case ROGUE_EQUIP_OFFHAND: return ROGUE_ITEM_ARMOR; /* placeholder: treat shields/foci as armor category until distinct */
		case ROGUE_EQUIP_ARMOR_HEAD: case ROGUE_EQUIP_ARMOR_CHEST: case ROGUE_EQUIP_ARMOR_LEGS:
		case ROGUE_EQUIP_ARMOR_HANDS: case ROGUE_EQUIP_ARMOR_FEET: return ROGUE_ITEM_ARMOR;
		case ROGUE_EQUIP_RING1: case ROGUE_EQUIP_RING2: case ROGUE_EQUIP_AMULET:
		case ROGUE_EQUIP_BELT: case ROGUE_EQUIP_CLOAK: case ROGUE_EQUIP_CHARM1: case ROGUE_EQUIP_CHARM2: return ROGUE_ITEM_ARMOR; /* reuse armor pipeline for now */
		default: return -1;
	}
}

int rogue_equip_item_is_two_handed(int inst_index){ const RogueItemInstance* it = rogue_item_instance_at(inst_index); if(!it) return 0; const RogueItemDef* d = rogue_item_def_at(it->def_index); if(!d) return 0; if(d->category!=ROGUE_ITEM_WEAPON) return 0; return (d->flags & ROGUE_ITEM_FLAG_TWO_HANDED) ? 1 : 0; }

static unsigned long long equip_mix64(unsigned long long h, unsigned long long v){ h ^= v + 0x9E3779B97F4A7C15ULL + (h<<6) + (h>>2); return h; }
int rogue_equip_try(enum RogueEquipSlot slot, int inst_index){
	const RogueItemInstance* it = rogue_item_instance_at(inst_index); if(!it) return -1; const RogueItemDef* d = rogue_item_def_at(it->def_index); if(!d) return -2; int want_cat = category_for_slot(slot); if(d->category != want_cat) return -3; 
	/* Two-handed weapon occupancy rule */
	if(slot==ROGUE_EQUIP_WEAPON){ if(rogue_equip_item_is_two_handed(inst_index)){ if(g_slots[ROGUE_EQUIP_OFFHAND]>=0) { g_slots[ROGUE_EQUIP_OFFHAND]=-1; } } }
	/* Equipping offhand while two-handed weapon equipped should fail */
	if(slot==ROGUE_EQUIP_OFFHAND){ int winst = g_slots[ROGUE_EQUIP_WEAPON]; if(winst>=0 && rogue_equip_item_is_two_handed(winst)) return -4; }
	g_slots[slot]=inst_index; 
	/* Phase 15.2: update item equip hash chain (equip event) */
	RogueItemInstance* mut=(RogueItemInstance*)it; mut->equip_hash_chain = equip_mix64(mut->equip_hash_chain, ((unsigned long long)slot<<56) ^ mut->guid ^ 0xE11AFBULL);
	rogue_stat_cache_mark_dirty();
	/* Immediate stat recompute so synchronous tests / UI observe updated totals */
	extern RoguePlayer g_exposed_player_for_stats; /* declared in app_state.h */
	rogue_equipment_apply_stat_bonuses(&g_exposed_player_for_stats);
	rogue_stat_cache_force_update(&g_exposed_player_for_stats);
	return 0; }
int rogue_equip_unequip(enum RogueEquipSlot slot){ if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; int prev=g_slots[slot]; g_slots[slot]=-1; if(prev>=0){ const RogueItemInstance* it=rogue_item_instance_at(prev); if(it){ RogueItemInstance* mut=(RogueItemInstance*)it; mut->equip_hash_chain = equip_mix64(mut->equip_hash_chain, ((unsigned long long)slot<<56) ^ mut->guid ^ 0x51CED9ULL); } rogue_stat_cache_mark_dirty(); extern RoguePlayer g_exposed_player_for_stats; rogue_equipment_apply_stat_bonuses(&g_exposed_player_for_stats); rogue_stat_cache_force_update(&g_exposed_player_for_stats); } return prev; }

int rogue_equip_set_transmog(enum RogueEquipSlot slot, int def_index){ if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; if(def_index<-1) return -2; if(def_index>=0){ const RogueItemDef* d = rogue_item_def_at(def_index); if(!d) return -3; /* Only allow same broad category for now */ int cat = category_for_slot(slot); if(d->category!=cat) return -4; }
	g_transmog_defs[slot]=def_index; return 0; }
int rogue_equip_get_transmog(enum RogueEquipSlot slot){ if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; return g_transmog_defs[slot]; }

int rogue_equip_repair_slot(enum RogueEquipSlot slot){
	if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; int inst = g_slots[slot]; if(inst<0) return -2; const RogueItemInstance* itc = rogue_item_instance_at(inst); if(!itc) return -3; if(itc->durability_max<=0) return 1; int missing = itc->durability_max - itc->durability_cur; if(missing<=0) return 1; const RogueItemDef* d = rogue_item_def_at(itc->def_index); if(!d) return -4; int rarity = d->rarity; int cost = rogue_econ_repair_cost_ex(missing, rarity, itc->item_level); if(rogue_econ_gold() < cost) return -5; rogue_econ_add_gold(-cost); rogue_item_instance_repair_full(inst); return 0; }
int rogue_equip_repair_all(void){ int repaired=0; for(int s=0;s<ROGUE_EQUIP__COUNT;s++){ int r=rogue_equip_repair_slot((enum RogueEquipSlot)s); if(r==0) repaired++; } return repaired; }
