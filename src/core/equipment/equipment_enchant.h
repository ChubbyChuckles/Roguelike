#ifndef ROGUE_EQUIPMENT_ENCHANT_H
#define ROGUE_EQUIPMENT_ENCHANT_H

/* Equipment Phase 5.4 / 5.5: Enchant & Reforge mechanics */

/* Reroll subset of affixes (prefix and/or suffix) preserving item_level, rarity, sockets & durability.
   Costs gold scaled by item_level & rarity and consumes an enchant material item (enchant_orb) if both affixes are rerolled.
   Returns 0 on success, negative on failure:
   -1 invalid item, -2 no such affix present to reroll, -3 insufficient gold, -4 missing material, -5 budget validation failure. */
int rogue_item_instance_enchant(int inst_index, int reroll_prefix, int reroll_suffix, int* out_cost);

/* Full reforge: completely reroll both affixes (if rarity allows) from scratch while preserving item_level & socket_count (and clearing existing sockets & gems).
   Consumes a reforge material (reforge_hammer) and gold. Returns 0 success; negatives mirror enchant plus -6 if material missing. */
int rogue_item_instance_reforge(int inst_index, int* out_cost);

/* Phase 5.6 (Optional now implemented): Protective seal to lock prefix and/or suffix during enchant.
   Consumes a protective_seal material item if at least one lock flag is set and item has that affix.
   Returns 0 success; -1 invalid item; -2 nothing to lock; -3 missing seal material. */
int rogue_item_instance_apply_protective_seal(int inst_index, int lock_prefix, int lock_suffix);

/* Query helpers for tests/UI */
int rogue_item_instance_is_prefix_locked(int inst_index);
int rogue_item_instance_is_suffix_locked(int inst_index);

#endif /* ROGUE_EQUIPMENT_ENCHANT_H */
