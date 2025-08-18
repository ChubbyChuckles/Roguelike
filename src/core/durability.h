/* Durability helpers (Equipment Phase 8) */
#ifndef ROGUE_DURABILITY_H
#define ROGUE_DURABILITY_H

/* Bucket classification used by UI & warning system.
   Returns 2 = good (>=60%), 1 = warn (>=30% <60%), 0 = critical (<30%). */
int rogue_durability_bucket(float pct);

/* Apply a durability event to an equipped item instance (weapons/armor).
   Severity typically corresponds to final damage dealt (weapon) or received (armor).
   Rarity influences scaling (higher rarity = slower loss).
   Returns durability loss applied (>=0). */
int rogue_item_instance_apply_durability_event(int inst_index, int severity);

#endif
