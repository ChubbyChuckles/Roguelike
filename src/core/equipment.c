#include "core/equipment.h"
#include "core/loot_instances.h"
#include "core/loot_item_defs.h"
#include "core/stat_cache.h"
#include "core/economy.h"
#include <string.h>

static int g_slots[ROGUE_EQUIP__COUNT];

void rogue_equip_reset(void){ for(int i=0;i<ROGUE_EQUIP__COUNT;i++) g_slots[i]=-1; }
int rogue_equip_get(enum RogueEquipSlot slot){ if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; return g_slots[slot]; }

static int category_for_slot(enum RogueEquipSlot slot){ switch(slot){ case ROGUE_EQUIP_WEAPON: return ROGUE_ITEM_WEAPON; case ROGUE_EQUIP_ARMOR: return ROGUE_ITEM_ARMOR; default: return -1; } }

int rogue_equip_try(enum RogueEquipSlot slot, int inst_index){ const RogueItemInstance* it = rogue_item_instance_at(inst_index); if(!it) return -1; const RogueItemDef* d = rogue_item_def_at(it->def_index); if(!d) return -2; int want_cat = category_for_slot(slot); if(d->category != want_cat) return -3; g_slots[slot]=inst_index; rogue_stat_cache_mark_dirty(); return 0; }
int rogue_equip_unequip(enum RogueEquipSlot slot){ if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; int prev=g_slots[slot]; g_slots[slot]=-1; if(prev>=0) rogue_stat_cache_mark_dirty(); return prev; }

int rogue_equip_repair_slot(enum RogueEquipSlot slot){
	if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; int inst = g_slots[slot]; if(inst<0) return -2; const RogueItemInstance* itc = rogue_item_instance_at(inst); if(!itc) return -3; if(itc->durability_max<=0) return 1; int missing = itc->durability_max - itc->durability_cur; if(missing<=0) return 1; const RogueItemDef* d = rogue_item_def_at(itc->def_index); if(!d) return -4; int rarity = d->rarity; int cost = rogue_econ_repair_cost(missing, rarity); if(rogue_econ_gold() < cost) return -5; rogue_econ_add_gold(-cost); rogue_item_instance_repair_full(inst); return 0; }
int rogue_equip_repair_all(void){ int repaired=0; for(int s=0;s<ROGUE_EQUIP__COUNT;s++){ int r=rogue_equip_repair_slot((enum RogueEquipSlot)s); if(r==0) repaired++; } return repaired; }
