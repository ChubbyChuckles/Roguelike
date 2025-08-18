/* Inventory Query & Advanced Sorting (Inventory System Phase 4.1-4.3)
 * Provides a lightweight expression parser over aggregated inventory definition entries.
 * Grammar (recursive descent, case-insensitive identifiers):
 *   expr := term { OR term }
 *   term := factor { AND factor }
 *   factor := '(' expr ')' | predicate
 *   predicate := IDENT OP VALUE
 *   IDENT := rarity | affix_weight | tag | equip_slot | quality | durability_pct | qty | quantity | category
 *   OP := = | != | < | <= | > | >= | ~ (substring / contains; only for tag & category string match)
 *   VALUE := integer | identifier (alnum / '_' ), or quoted string "..." for tag/category
 * Notes:
 * - Aggregated inventory (def->qty) maps instance-derived predicates (affix_weight, quality, durability_pct)
 *   with ANY-instance semantics: predicate true if at least one active item instance of the definition satisfies condition.
 * - equip_slot currently matches by category heuristic (weapon -> WEAPON, others -> ARMOR or MISC).
 * - category value accepts numeric or string: misc, consumable, weapon, armor, gem, material.
 * - tag '=' means tag present (exact), '!=' tag absent, '~' substring case-insensitive across any tag.
 * - qty and quantity are aliases.
 */
#ifndef ROGUE_INVENTORY_QUERY_H
#define ROGUE_INVENTORY_QUERY_H

#include <stdint.h>
#include <stddef.h> /* size_t */
#include <stdio.h>  /* FILE */
#ifdef __cplusplus
extern "C" {
#endif

/* Execute query expression, writing matching definition indices (unsorted). */
int rogue_inventory_query_execute(const char* expr, int* out_def_indices, int cap);

/* Composite sort: comma-separated key list, key may start with '-' for descending.
 * Supported keys: rarity, qty, quantity (alias), name, category.
 */
int rogue_inventory_query_sort(int* def_indices, int count, const char* sort_keys_csv);

/* Fuzzy text search over definition names (trigram containment). */
int rogue_inventory_fuzzy_search(const char* text, int* out_def_indices, int cap);
int rogue_inventory_fuzzy_rebuild_index(void);

/* Saved searches (Phase 4.4): store up to N named query+sort expressions persisted via save component.
 * API returns 0 success / -1 failure. Names case-insensitive unique.
 */
int rogue_inventory_saved_search_store(const char* name, const char* query_expr, const char* sort_keys);
int rogue_inventory_saved_search_get(const char* name, char* out_query, int qcap, char* out_sort, int scap);
int rogue_inventory_saved_search_count(void);
int rogue_inventory_saved_search_name(int index, char* out_name, int cap);

/* Persistence (Phase 4.4): write/read saved searches as component id ROGUE_SAVE_COMP_INV_SAVED_SEARCHES */
int rogue_inventory_saved_searches_write(FILE* f); /* returns 0 */
int rogue_inventory_saved_searches_read(FILE* f, size_t size); /* returns 0 */


/* Mutation hook: call when item instance metadata (affix weight / quality / durability) changes so we can re-index fuzzy needed sets (Phase 4.5). */
void rogue_inventory_query_on_instance_mutation(int inst_index);


#ifdef __cplusplus
}
#endif
#endif /* ROGUE_INVENTORY_QUERY_H */
