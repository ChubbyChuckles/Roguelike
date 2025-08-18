#ifndef ROGUE_EQUIPMENT_PERSIST_H
#define ROGUE_EQUIPMENT_PERSIST_H
#include <stddef.h>
/* Phase 13: Versioned equipment block serialization & integrity hashing */
#ifdef __cplusplus
extern "C" {
#endif

#define ROGUE_EQUIP_SCHEMA_VERSION 1

/* Serialize current equipment state (only equipped items) into text buffer.
   Returns bytes written (excluding NUL) or -1 on error. Format (V1):
   EQUIP_V1\n
   SLOT <slot_index> DEF <def_index> ILVL <item_level> RAR <rarity> PREF <pidx> <pval> SUFF <sidx> <sval> DUR <cur> <max> ENCH <ench> QC <quality> SOCKS <count> <g0> <g1> <g2> <g3> <g4> <g5> LOCKS <pl> <sl> FRACT <fractured>\n
   (One line per occupied slot; unused sockets gX=-1). */
int rogue_equipment_serialize(char* buf, int cap);

/* Deserialize equipment block produced by serialize. Accepts legacy (v0) format where lines are:
   SLOT <slot_index> DEF <def_index>\n (no header). Returns 0 on success. */
int rogue_equipment_deserialize(const char* buf);

/* Deterministic 64-bit FNV-1a hash over serialized equipment state (V1). Returns 0 on error. */
unsigned long long rogue_equipment_state_hash(void);

#ifdef __cplusplus
}
#endif
#endif
