/* Crafting & Gathering Phase 9 Automation & Smart Assist (9.1-9.5)
 * Provides helper planning / recommendation utilities.
 * Deterministic, side-effect free analysis helpers.
 */
#ifndef ROGUE_CRAFTING_AUTOMATION_H
#define ROGUE_CRAFTING_AUTOMATION_H

#ifdef __cplusplus
extern "C" {
#endif

struct RogueCraftRecipe; /* forward decl */

typedef struct RogueCraftPlanEntry { int def_index; int needed; int have; int missing; } RogueCraftPlanEntry;
typedef struct RogueCraftPlan { RogueCraftPlanEntry entries[8]; int entry_count; int total_missing; } RogueCraftPlan;
int rogue_craft_plan_build(const struct RogueCraftRecipe* recipe, int batch_qty, RogueCraftPlan* out);

int rogue_craft_suggest_gather_routes(const RogueCraftPlan* plan, int max_indices, int* out_node_def_indices);

typedef struct RogueRefineSuggestion { int material_def; int from_quality; int to_quality; int consume_count; } RogueRefineSuggestion;
int rogue_craft_suggest_refine(RogueRefineSuggestion* out, int max);

int rogue_craft_decision_score(const struct RogueCraftRecipe* recipe);
int rogue_craft_idle_recommendation(char* buf, int buf_sz);

#ifdef __cplusplus
}
#endif

#endif
