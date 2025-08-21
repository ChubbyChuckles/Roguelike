#ifndef ROGUE_LOOT_FILTER_H
#define ROGUE_LOOT_FILTER_H
#include "core/loot/loot_item_defs.h"
#ifdef __cplusplus
extern "C" { 
#endif

/* Loot Filter System (12.1–12.2)
 * Provides parsing of simple rule config and predicate evaluation used to hide ground items.
 * Rule syntax (one per line, case-insensitive ids):
 *   rarity>=N        (0..4)
 *   rarity<=N
 *   category=weapon  (weapon,armor,material,consumable,other numeric fallback)
 *   name~substr      (substring match, lowercase compare)
 *   def=id_exact     (exact item id)
 * Lines starting with # ignored. Blank lines ignored.
 * Rules are ANDed together by default.
 * Special line: MODE=ANY switches logical combination to OR until MODE=ALL appears.
 */

int rogue_loot_filter_reset(void);
int rogue_loot_filter_load(const char* path); /* returns number rules loaded */
int rogue_loot_filter_rule_count(void);
/* Returns 1 if item def is allowed (visible), 0 if filtered out, -1 on error */
int rogue_loot_filter_match(const RogueItemDef* def);
/* Force re-evaluation of existing item instances (calls rogue_items_reapply_filter) */
void rogue_loot_filter_refresh_instances(void);

#ifdef __cplusplus
}
#endif
#endif
