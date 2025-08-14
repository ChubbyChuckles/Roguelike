/* Equipment Slots (14.1 initial) */
#ifndef ROGUE_EQUIPMENT_H
#define ROGUE_EQUIPMENT_H

enum RogueEquipSlot { ROGUE_EQUIP_WEAPON=0, ROGUE_EQUIP_ARMOR=1, ROGUE_EQUIP__COUNT };

void rogue_equip_reset(void);
int rogue_equip_get(enum RogueEquipSlot slot); /* returns item instance index or -1 */
int rogue_equip_try(enum RogueEquipSlot slot, int inst_index); /* 0 on success */
int rogue_equip_unequip(enum RogueEquipSlot slot); /* returns previous inst index */

#endif
