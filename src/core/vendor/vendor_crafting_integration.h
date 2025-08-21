/* Vendor System Phase 8: Integration With Crafting & Gathering */
#ifndef ROGUE_VENDOR_CRAFTING_INTEGRATION_H
#define ROGUE_VENDOR_CRAFTING_INTEGRATION_H
#ifdef __cplusplus
extern "C"
{
#endif

    /* 8.1 Purchase unlock for crafting recipes: vendor sells blueprint tokens that, when purchased,
           mark the recipe as discovered (if not already). */
    int rogue_vendor_purchase_recipe_unlock(int recipe_index, int gold_cost,
                                            int (*spend_gold_cb)(int gold),
                                            void (*on_unlocked_cb)(int recipe_index));

    /* 8.3 Batch refinement service: vendor performs a refinement operation across multiple
       materials. Applies a gold service fee (percent of base value sum) and aggregates outcomes.
           Parameters:
             material_def_index: material to refine
             from_quality: source quality bucket
             to_quality: target quality bucket (higher)
             batch_count: how many refinement attempts (each consumes consume_count units)
             consume_count: units consumed per attempt
             gold_fee_pct: fee percent (e.g., 15 => 15%) applied to base value of consumed materials
             base_value: economic base value per unit (fallback if not from registry)
             spend_gold_cb: spend fee
           Returns total promoted units (sum of success + crit extra) or <0 on error.
    */
    int rogue_vendor_batch_refine(int material_def_index, int from_quality, int to_quality,
                                  int batch_count, int consume_count, int gold_fee_pct,
                                  int base_value, int (*spend_gold_cb)(int gold));

    /* 8.4 Scarcity feedback loop: record vendor-observed deficit for a material (e.g., many
       purchase attempts, few sales). This accumulates a simple deficit counter which gathering
       spawn weighting code may query. deficit_delta can be negative to reduce.
    */
    void rogue_vendor_scarcity_record(int material_def_index, int deficit_delta);
    int rogue_vendor_scarcity_score(int material_def_index); /* returns current deficit score */

#ifdef __cplusplus
}
#endif
#endif
