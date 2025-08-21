#ifndef ROGUE_EQUIPMENT_ENHANCE_H
#define ROGUE_EQUIPMENT_ENHANCE_H

/* Phase 6: Item Modification & Enhancement Pathways
   Imbue: add new affix (prefix or suffix) if slot empty, consuming catalyst material (optional if
   material found). Temper: raise existing affix value within remaining budget. Includes fracture
   (failure) risk causing durability damage. Socket crafting: add socket (up to item def max) or
   reroll socket layout in its min..max range. Fracture risk: on temper failure durability damage is
   applied; reaching 0 durability sets fractured flag (existing durability system).

   All operations are deterministic in sequence order via a local LCG state; Phase 7 will separate
   RNG streams.
*/

#ifdef __cplusplus
extern "C"
{
#endif

    /* Imbue a new affix. is_prefix!=0 selects prefix slot else suffix. Returns 0 on success.
       Negative codes: -1 invalid item, -2 slot occupied, -3 no remaining budget, -4 affix roll
       failed, -5 catalyst missing (if catalyst definition present), -6 budget clamp produced zero
       value. */
    int rogue_item_instance_imbue(int inst_index, int is_prefix, int* out_affix_index,
                                  int* out_affix_value);

    /* Temper (increment) an existing affix value by up to intensity (>=1) within remaining budget.
       Success chance currently fixed at 80%; on failure durability damage is applied (failure
       damage = 5 + intensity). Returns 0 on success, 1 on no-op (already at budget cap), 2 on
       failure with durability damage. Negative codes: -1 invalid item, -2 missing affix, -3
       intensity invalid. out_new_value (optional) receives updated value (or unchanged on failure),
       out_failed set to 1 on failure. */
    int rogue_item_instance_temper(int inst_index, int is_prefix, int intensity, int* out_new_value,
                                   int* out_failed);

    /* Add a single socket if below item definition socket_max. Returns 0 on success, 1 if already
       at max. Negative codes: -1 invalid item/def, -2 max sockets <=0 (unsupported def), -3
       catalyst missing if required. */
    int rogue_item_instance_add_socket(int inst_index, int* out_new_count);

    /* Reroll socket layout (count) within [socket_min, socket_max]. Clears existing inserted gems.
       Returns 0 on success, -1 invalid, -2 range invalid or max<=0, -3 catalyst missing. */
    int rogue_item_instance_reroll_sockets(int inst_index, int* out_new_count);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_EQUIPMENT_ENHANCE_H */
