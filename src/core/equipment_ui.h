/* Equipment System Phase 12: UI / Visualization helpers (text only for tests)
   Provides grouped panel builder, layered tooltip, comparison deltas, proc preview,
   socket drag/drop ephemeral state, and transmog selection tracking wrappers.
   Deterministic string outputs used by unit tests (no renderer dependency). */
#ifndef ROGUE_EQUIPMENT_UI_H
#define ROGUE_EQUIPMENT_UI_H
#include <stddef.h>
#include "core/equipment.h"

#ifdef __cplusplus
extern "C" { 
#endif

char* rogue_equipment_panel_build(char* buf, size_t cap);
char* rogue_item_tooltip_build_layered(int inst_index, int compare_slot, char* buf, size_t cap);
char* rogue_equipment_compare_deltas(int inst_index, int compare_slot, char* buf, size_t cap);
float rogue_equipment_proc_preview_dps(void);
int rogue_equipment_socket_select(int inst_index, int socket_index);
int rogue_equipment_socket_place_gem(int gem_item_def_index);
void rogue_equipment_socket_clear_selection(void);
int rogue_equipment_transmog_select(enum RogueEquipSlot slot, int def_index);
int rogue_equipment_transmog_last_selected(enum RogueEquipSlot slot);
unsigned int rogue_item_tooltip_hash(int inst_index, int compare_slot);

#ifdef __cplusplus
}
#endif
#endif
