#ifndef ROGUE_INVENTORY_ENTRIES_H
#define ROGUE_INVENTORY_ENTRIES_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" { 
#endif
/* Phase 1 unified entry governance (def_id -> quantity) + labels + cap policy + delta tracking */
#define ROGUE_INV_MAX_ENTRIES 4096
#define ROGUE_INV_ERR_UNIQUE_CAP   -1000
#define ROGUE_INV_ERR_OVERFLOW     -1001

/* Logical compartment labels (Phase 1.3). Pure metadata for UI grouping; not storage separation. */
#define ROGUE_INV_LABEL_MATERIAL 0x1u
#define ROGUE_INV_LABEL_QUEST    0x2u
#define ROGUE_INV_LABEL_GEAR     0x4u

/* Delta block magic (Phase 1.6) written into inventory save component when there are changed entries. */
#define ROGUE_INV_DELTA_MAGIC 0x31445649u /* "IVD1" little-endian */

typedef int (*RogueInventoryCapHandler)(int def_index, uint64_t add_qty); /* Phase 1.7: called when unique cap would be exceeded; return 0 to retry pickup after performing mitigation (e.g., salvage/remove), else negative to abort. */

int rogue_inventory_entries_init(void); /* resets state */
int rogue_inventory_set_unique_cap(unsigned cap); /* soft cap (# distinct defs) */
unsigned rogue_inventory_unique_cap(void);
unsigned rogue_inventory_unique_count(void); /* current distinct defs */
uint64_t rogue_inventory_quantity(int def_index); /* 0 if none */
/* Pressure: fraction [0,1]; 0 if cap==0 or empty. */
double rogue_inventory_entry_pressure(void);
/* Returns 0 if acceptable to add (may create new distinct entry), else negative error code. */
int rogue_inventory_can_accept(int def_index, uint64_t add_qty);
/* Apply a pickup (add_qty>=1). Performs overflow & cap enforcement. Returns 0 or error. */
int rogue_inventory_register_pickup(int def_index, uint64_t add_qty);
/* Apply removal (salvage/drop). Quantity decremented; distinct entry removed when reaches 0. */
int rogue_inventory_register_remove(int def_index, uint64_t remove_qty);
/* Label metadata (bitmask of ROGUE_INV_LABEL_*). Setting labels on a non-existent entry has no effect (returns -1). */
int rogue_inventory_entry_set_labels(int def_index, unsigned labels);
unsigned rogue_inventory_entry_labels(int def_index);
/* Phase 1.7: install cap handler (optional). Returns 0. */
int rogue_inventory_set_cap_handler(RogueInventoryCapHandler fn);
/* Phase 1.6: enumerate dirty (changed since last snapshot) entries. Returns count copied (<=cap). Quantities array receives current absolute quantity (0 means removed). Passing NULL resets tracking without enumeration. */
unsigned rogue_inventory_entries_dirty_pairs(int* out_def_indices, uint64_t* out_quantities, unsigned cap);
/* Internal/tests: clear dirty tracking (treat current state as baseline). */
void rogue_inventory_entries_clear_dirty(void);
#ifdef __cplusplus
}
#endif
#endif
