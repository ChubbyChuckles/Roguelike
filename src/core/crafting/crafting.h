/* Crafting & Materials (11.2 - 11.5)
 * Provides:
 *  - Material rarity tier queries (11.2)
 *  - Crafting recipe registry & parsing from cfg (11.3)
 *  - Upgrade path API (11.4)
 *  - Affix reroll API integrating economy + materials (11.5)
 */
#ifndef ROGUE_CRAFTING_H
#define ROGUE_CRAFTING_H

#include "../loot/loot_item_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Material tier classification (simple mapping by item rarity field) */
    /* Returns 0=common,1=uncommon,2=rare,3=epic,4=legendary for material def, or -1 if def_index
     * invalid/not material. */
    int rogue_material_tier(int def_index);

    /* Crafting Recipes */
    typedef struct RogueCraftIngredient
    {
        int def_index;
        int quantity;
    } RogueCraftIngredient;
    typedef struct RogueCraftRecipe
    {
        char id[32];
        int output_def; /* item definition produced */
        int output_qty;
        RogueCraftIngredient inputs[6]; /* up to 6 ingredients */
        int input_count;
        int upgrade_source_def;   /* if this is an upgrade path transforming an existing item */
        int rarity_upgrade_delta; /* +rarity applied to upgraded item (11.4) */
        int time_ms;              /* Phase 4.1: crafting time requirement */
        char station[24];         /* Phase 4.1: required station tag
                                     (forge/alchemy_table/workbench/mystic_altar) */
        int skill_req;            /* Phase 4.1: minimum crafting skill required */
        int exp_reward;           /* Phase 4.1: XP rewarded on completion */
    } RogueCraftRecipe;

    int rogue_craft_reset(void);
    int rogue_craft_recipe_count(void);
    const RogueCraftRecipe* rogue_craft_recipe_at(int index);
    const RogueCraftRecipe* rogue_craft_find(const char* id);
    /* Parse a recipe config file. Format lines:
     * id,output_id,output_qty, input_id:qty;input_id:qty;..., [upgrade:source_id:+rarity]
     * Lines starting with # ignored. Empty -> skip.
     * Returns number of recipes added or <0 on error. */
    int rogue_craft_load_file(const char* path);
    /* JSON loader for recipes: array of objects with fields:
        id, output (string id), output_qty, inputs (array of {id,qty}), optional upgrade {source,
       rarity_delta}, optional time_ms, station, skill_req, exp_reward. */
    int rogue_craft_load_json(const char* path);

    /* Validation helpers (Phase 2.3.3.5-.7 and 2.3.3.6):
        - dependency: ensure all ingredient/output item defs exist (returns count of bad refs)
        - balance: compute total base_value(inputs) vs output base value and return number of
       out-of-bounds recipes (>ratio_max or <ratio_min) */
    int rogue_craft_validate_dependencies(void);
    int rogue_craft_validate_balance(float ratio_min, float ratio_max);
    /* Ensure skill requirement is non-decreasing across upgrade chains and within reasonable bounds
     * (0..100); returns count fixed or violations */
    int rogue_craft_validate_skill_requirements(void);

    /* Attempt to craft by recipe id: validates inventory counts using provided callbacks. */
    typedef int (*RogueInvGetFn)(int def_index);
    typedef int (*RogueInvAddFn)(int def_index, int qty);
    /* Optional removal callback; if NULL uses add with negative which we currently do not support
     * -> then fails. */
    typedef int (*RogueInvConsumeFn)(int def_index, int qty);
    int rogue_craft_execute(const RogueCraftRecipe* r, RogueInvGetFn inv_get,
                            RogueInvConsumeFn inv_consume, RogueInvAddFn inv_add);

    /* Upgrade path: given a base item definition index and recipe specifying rarity delta, returns
     * new rarity clamped. */
    int rogue_craft_apply_upgrade(int base_rarity, int rarity_delta);

    /* Affix reroll using currency + materials (11.5). Requires an inventory remove implementation.
     * Returns 0 success, <0 on failure. Affix regeneration done via callback to core item instance
     * API. */
    typedef int (*RogueAffixRerollFn)(int inst_index, unsigned int* rng_state, int rarity);
    int rogue_craft_reroll_affixes(int inst_index, int rarity, int material_def_index,
                                   int material_cost, int gold_cost, RogueInvGetFn inv_get,
                                   RogueInvConsumeFn inv_consume, int (*gold_spend_fn)(int amount),
                                   RogueAffixRerollFn reroll_fn, unsigned int* rng_state);

    /* Phase 10.5 (Optional): Crafting success chance scaling with player crafting skill.
     * Skill is an integer (0+). Success % formula (subject to tuning):
     *   base 35% + skill*4% - rarity*5% - difficulty*3% (clamped 5%..95%).
     * Exposed APIs allow deterministic tests (caller supplies rng_state).
     */
    void rogue_craft_set_skill(int skill);
    int rogue_craft_get_skill(void);
    /* Returns success (1) or failure (0). base_rarity influences difficulty. */
    int rogue_craft_success_attempt(int base_rarity, int difficulty, unsigned int* rng_state);
    /* Convenience: attempt an upgrade stone application gated by success; returns 0 success, >0
     * fail code (1=fail), <0 error. */
    int rogue_craft_attempt_upgrade(int inst_index, int tiers, int difficulty,
                                    unsigned int* rng_state);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CRAFTING_H */
