/* Crafting & Gathering Phase 10 Economy & Balance Hooks
 * Features implemented:
 * 10.1 Material drop rate adjustment via dynamic weights (scarcity feedback)
 * 10.2 Crafting inflation guard (diminishing XP returns on repetitive low-tier crafts)
 * 10.3 Soft caps for high-tier material accumulation (recommend refine/spend suggestions)
 * 10.4 Value model extension integrating material quality & rarity adjustment for outputs
 * 10.5 Tests (unit test validates core behaviors)
 */
#ifndef ROGUE_CRAFTING_ECONOMY_H
#define ROGUE_CRAFTING_ECONOMY_H
#ifdef __cplusplus
extern "C" { 
#endif

/* Scarcity metric per material (derived: required_from_recipes - inventory_have) / (have+1). */
float rogue_craft_material_scarcity(int item_def_index);
/* Returns dynamic spawn weight scalar for a material (1.0 baseline) clamped [0.75,1.35]. */
float rogue_craft_dynamic_spawn_scalar(int item_def_index);

/* Soft cap pressure (0..1) when material total quantity surpasses threshold (tier scaled). */
float rogue_craft_material_softcap_pressure(int item_def_index);

/* Inflation guard XP scaling factor for a recipe given recent craft count of same recipe. Range [0.25,1]. */
float rogue_craft_inflation_xp_scalar(int recipe_index);
/* Call when a recipe is completed to update inflation counters (time_ms real time not needed yet). */
void  rogue_craft_inflation_on_craft(int recipe_index);
void  rogue_craft_inflation_decay_tick(void); /* passive decay each session tick call */

/* Extended item output value factoring material quality bias & rarity delta (Phase 10.4). */
int rogue_craft_enhanced_item_value(int def_index, int rarity, int affix_power_raw, float durability_fraction, float material_quality_bias);

#ifdef __cplusplus
}
#endif
#endif
