/* Equipment Slots (14.1 initial) */
#ifndef ROGUE_EQUIPMENT_H
#define ROGUE_EQUIPMENT_H

/* Expanded slot model (Phase Equipment 1): weapon + armor + jewelry + utility + charms + offhand */
enum RogueEquipSlot
{
    ROGUE_EQUIP_WEAPON = 0,
    ROGUE_EQUIP_OFFHAND = 1, /* shield / focus / quiver */
    ROGUE_EQUIP_ARMOR_HEAD = 2,
    ROGUE_EQUIP_ARMOR_CHEST = 3,
    ROGUE_EQUIP_ARMOR_LEGS = 4,
    ROGUE_EQUIP_ARMOR_HANDS = 5,
    ROGUE_EQUIP_ARMOR_FEET = 6,
    ROGUE_EQUIP_RING1 = 7,
    ROGUE_EQUIP_RING2 = 8,
    ROGUE_EQUIP_AMULET = 9,
    ROGUE_EQUIP_BELT = 10,
    ROGUE_EQUIP_CLOAK = 11,
    ROGUE_EQUIP_CHARM1 = 12,
    ROGUE_EQUIP_CHARM2 = 13,
    ROGUE_EQUIP__COUNT
};

#define ROGUE_EQUIP_SLOT_COUNT ROGUE_EQUIP__COUNT

/* Slot metadata flags */
#define ROGUE_EQUIP_META_FLAG_TWOHAND 0x01 /* weapon instance occupies offhand */

/* Public query describing current slot occupancy rules for an item instance. If returns 1 for
 * two_handed, OFFHAND will be cleared automatically on equip. */
int rogue_equip_item_is_two_handed(int inst_index);

void rogue_equip_reset(void);
int rogue_equip_get(enum RogueEquipSlot slot); /* returns item instance index or -1 */
int rogue_equip_try(enum RogueEquipSlot slot, int inst_index); /* 0 on success */
int rogue_equip_unequip(enum RogueEquipSlot slot);             /* returns previous inst index */

/* Convenience wrapper (Phase 5 tests) aliasing rogue_equip_try for naming consistency */
static inline int rogue_equip_equip(enum RogueEquipSlot slot, int inst_index)
{
    return rogue_equip_try(slot, inst_index);
}

/* Repair utilities (10.4 integration) */
/* Attempts to fully repair the item in slot; spends gold via economy; returns 0 success, <0 failure
 * (no item or insufficient gold). */
int rogue_equip_repair_slot(enum RogueEquipSlot slot);
/* Repairs all equipped items, returns count repaired. */
int rogue_equip_repair_all(void);

/* Cosmetic transmog layer (Phase 1.5) */
int rogue_equip_set_transmog(
    enum RogueEquipSlot slot,
    int def_index); /* def_index: base item definition for visual override (-1 clears) */
int rogue_equip_get_transmog(enum RogueEquipSlot slot); /* returns def_index or -1 if none */

#endif
