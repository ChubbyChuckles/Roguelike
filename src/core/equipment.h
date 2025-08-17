/* Equipment Slots (14.1 initial) */
#ifndef ROGUE_EQUIPMENT_H
#define ROGUE_EQUIPMENT_H

/* Expanded slot model (14.1): weapon + five armor slots */
enum RogueEquipSlot { ROGUE_EQUIP_WEAPON=0, ROGUE_EQUIP_ARMOR_HEAD=1, ROGUE_EQUIP_ARMOR_CHEST=2, ROGUE_EQUIP_ARMOR_LEGS=3, ROGUE_EQUIP_ARMOR_HANDS=4, ROGUE_EQUIP_ARMOR_FEET=5, ROGUE_EQUIP__COUNT };

void rogue_equip_reset(void);
int rogue_equip_get(enum RogueEquipSlot slot); /* returns item instance index or -1 */
int rogue_equip_try(enum RogueEquipSlot slot, int inst_index); /* 0 on success */
int rogue_equip_unequip(enum RogueEquipSlot slot); /* returns previous inst index */

/* Repair utilities (10.4 integration) */
/* Attempts to fully repair the item in slot; spends gold via economy; returns 0 success, <0 failure (no item or insufficient gold). */
int rogue_equip_repair_slot(enum RogueEquipSlot slot);
/* Repairs all equipped items, returns count repaired. */
int rogue_equip_repair_all(void);

#endif
