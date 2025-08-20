/* Crafting & Gathering Phase 9 Automation & Smart Assist
 * Implements features 9.1–9.6:
 *  9.1 Craft planner (goal recipe -> aggregated required inputs vs inventory)
 *  9.2 Gathering route suggestion (heuristic ordering of node defs by scarcity coverage)
 *  9.3 Auto-refine suggestion queue (identify profitable quality promotions)
 *  9.4 Salvage vs craft decision scoring (expected value delta)
 *  9.5 Idle recommendation panel (what material to gather next)
 *  9.6 Tests (implemented in unit test file)
 *
 * The intent is to provide purely deterministic, side‑effect free advisory helpers that
 * can be consumed by UI or future AI agents. No inventory mutation is performed here.
 */
#ifndef ROGUE_CRAFTING_AUTOMATION_H
#define ROGUE_CRAFTING_AUTOMATION_H

#ifdef __cplusplus
extern "C" { 
#endif

#include <stddef.h>

typedef struct RogueCraftRecipe RogueCraftRecipe; /* forward */

/* Requirement entry for planner */
typedef struct RogueCraftPlanReq {
    int item_def;    /* item definition index */
    int required;    /* total required units */
    int have;        /* current inventory count */
    int missing;     /* max(0, required - have) */
} RogueCraftPlanReq;

/* 9.1 Craft planner
 * Computes aggregated input requirements for a target recipe * batch_qty. If recursive is non‑zero,
 * attempts to expand inputs that themselves are craftable by other recipes (one level per recursion depth),
 * up to max_depth (cycle safe). Returns count of populated entries (<= max_out). Deterministic ordering
 * by ascending item_def.
 */
int rogue_craft_plan_requirements(const RogueCraftRecipe* recipe, int batch_qty,
                                  int recursive, int max_depth,
                                  RogueCraftPlanReq* out, int max_out);

/* 9.2 Gathering route suggestion
 * Produces ordered list of gathering node definition indices prioritized by scarcity weight =
 * sum_over_materials( scarcity_ratio(material) * node_weight_fraction ). scarcity_ratio = missing/(have+1).
 * Returns count written (<= max_out). If no deficits, returns 0.
 */
int rogue_craft_gather_route(int* out_node_defs, int max_out);

/* Refine suggestion entry (9.3) */
typedef struct RogueRefineSuggestion {
    int material_def;
    int from_quality;   /* source bucket */
    int to_quality;     /* target bucket */
    int consume_count;  /* units to consume */
    int est_produced;   /* estimated produced units (EV, integer) */
} RogueRefineSuggestion;

/* Generate at most max_out suggestions. Heuristic: for each material whose average quality < avg_threshold
 * and with >= min_bucket units at from_quality (default bucket 0..40 scanned), suggest a refine upward by delta_q.
 * Returns count. Deterministic ordering by material_def ascending then from_quality.
 */
int rogue_craft_refine_suggestions(int avg_threshold, int min_bucket, int delta_q,
                                   RogueRefineSuggestion* out, int max_out);

/* 9.4 Salvage vs Craft decision
 * Scores expected value of salvaging an item vs crafting an upgrade recipe (cost includes input material base values).
 * Returns 1 if crafting upgrade is recommended, 0 otherwise. Negative on error. Outputs raw values if pointers non‑NULL.
 */
int rogue_craft_decision_salvage_vs_craft(int item_def_index, int item_rarity,
                                          const RogueCraftRecipe* upgrade_recipe,
                                          double* out_salvage_value,
                                          double* out_craft_net_gain);

/* 9.5 Idle material recommendation (what to gather next)
 * Returns material_def with highest scarcity ratio among materials appearing in any recipe inputs.
 * Returns -1 if none missing. */
int rogue_craft_idle_recommend_material(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CRAFTING_AUTOMATION_H */
