/* Item tooltip builder (Itemsystem 12.5/12.6) */
#ifndef ROGUE_LOOT_TOOLTIP_H
#define ROGUE_LOOT_TOOLTIP_H
#include <stddef.h>

/* Build a tooltip string for item instance index (ground or equipped). Returns buf on success or
 * NULL. */
char* rogue_item_tooltip_build(int inst_index, char* buf, size_t buf_sz);
/* Build comparison tooltip between candidate instance and equipped slot (weapon/armor).
 * compare_slot = -1 to skip. */
char* rogue_item_tooltip_build_compare(int inst_index, int compare_slot, char* buf, size_t buf_sz);

#endif
