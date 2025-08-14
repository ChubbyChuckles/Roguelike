#include "core/equipment.h"
#include "core/loot_instances.h"
#include "core/loot_item_defs.h"
#include <string.h>

static int g_slots[ROGUE_EQUIP__COUNT];

void rogue_equip_reset(void){ for(int i=0;i<ROGUE_EQUIP__COUNT;i++) g_slots[i]=-1; }
int rogue_equip_get(enum RogueEquipSlot slot){ if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; return g_slots[slot]; }

static int category_for_slot(enum RogueEquipSlot slot){ switch(slot){ case ROGUE_EQUIP_WEAPON: return ROGUE_ITEM_WEAPON; case ROGUE_EQUIP_ARMOR: return ROGUE_ITEM_ARMOR; default: return -1; } }

int rogue_equip_try(enum RogueEquipSlot slot, int inst_index){ const RogueItemInstance* it = rogue_item_instance_at(inst_index); if(!it) return -1; const RogueItemDef* d = rogue_item_def_at(it->def_index); if(!d) return -2; int want_cat = category_for_slot(slot); if(d->category != want_cat) return -3; g_slots[slot]=inst_index; return 0; }
int rogue_equip_unequip(enum RogueEquipSlot slot){ if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; int prev=g_slots[slot]; g_slots[slot]=-1; return prev; }
