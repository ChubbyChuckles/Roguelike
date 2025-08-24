/**
 * @file crafting_economy.c
 * @brief Crafting economy helpers: inflation guards, scarcity, and value model.
 *
 * Contains tuning and lightweight runtime helpers used by crafting to
 * avoid runaway inflation (recent craft decay), map scarcity to
 * dynamic spawn scalars, compute softcap pressure and produce an
 * enhanced item value estimate that composes base econ_value with
 * material/rarity/affix considerations.
 */
#include "crafting_economy.h"
#include "../inventory/inventory.h" /* assumed existing for count accessor prototypes */
#include "../loot/loot_item_defs.h"
#include "../vendor/econ_value.h"
#include "crafting.h"
#include <math.h>
#include <string.h>

/* Internal lightweight state. We keep recent craft counts for inflation guard. */
#ifndef MAX_RECIPES_TRACKED
#define MAX_RECIPES_TRACKED 2048
#endif
static unsigned short s_recent_craft_counts[MAX_RECIPES_TRACKED];

/* Decay counters gradually (called maybe once per in-game minute or per test). */
/**
 * @brief Decay recent craft counters gradually.
 *
 * Intended to be called periodically (eg once per in-game minute or
 * test tick). Applies a 25% decay to each tracked recipe's recent
 * craft counter to model cooling demand and prevent permanent
 * inflation adjustments.
 */
void rogue_craft_inflation_decay_tick(void)
{
    for (int i = 0; i < MAX_RECIPES_TRACKED; i++)
    {
        unsigned short c = s_recent_craft_counts[i];
        if (c > 0)
        {
            c = (unsigned short) (c * 3 / 4); /* 25% decay */
            if (c == 0)
                c = 0; /* explicit */
            s_recent_craft_counts[i] = c;
        }
    }
}

/**
 * @brief Increment recent craft counter for a recipe.
 *
 * Bounds-checked against internal tracking capacity. The cap guards
 * against overflow in long-running sessions.
 *
 * @param recipe_index Index of the crafted recipe
 */
void rogue_craft_inflation_on_craft(int recipe_index)
{
    if (recipe_index >= 0 && recipe_index < MAX_RECIPES_TRACKED)
    {
        if (s_recent_craft_counts[recipe_index] < 60000)
        {
            s_recent_craft_counts[recipe_index]++;
        }
    }
}

/**
 * @brief Compute an XP scalar based on recent craft frequency.
 *
 * Implements a diminishing-return scalar that reduces XP provided
 * by repeated crafting of the same recipe to mitigate grinding
 * inflation. Returns 1.0 for new recipes and clamps to [0.25,1.0].
 *
 * @param recipe_index Index of recipe to evaluate
 * @return XP multiplier
 */
float rogue_craft_inflation_xp_scalar(int recipe_index)
{
    if (recipe_index < 0 || recipe_index >= MAX_RECIPES_TRACKED)
        return 1.0f;
    unsigned short c = s_recent_craft_counts[recipe_index];
    if (c <= 3)
        return 1.0f; /* first few full value */
    /* Diminishing: scalar = max(0.25, (sqrt(8)/sqrt(c+1))) normalized to start near 1 at c=4 */
    float scalar = (float) (2.8284271247461903 / sqrtf((float) (c + 1))); /* sqrt(8)=2.828... */
    if (scalar > 1.0f)
        scalar = 1.0f; /* early stage bound */
    if (scalar < 0.25f)
        scalar = 0.25f;
    return scalar;
}

/* Scarcity estimation: look over all recipes and compute net demand for this material's def id. */
/**
 * @brief Estimate scarcity of an item by aggregating recipe demand.
 *
 * Scans registered craft recipes and sums the absolute input
 * demand for `item_def_index`. This naive estimator returns
 * deficit/(have+1) where `have` currently uses a platform
 * inventory accessor when available. Returns 0 when no scarcity.
 *
 * @param item_def_index Item definition index to evaluate
 * @return Scarcity score (>=0)
 */
float rogue_craft_material_scarcity(int item_def_index)
{
    if (item_def_index < 0)
        return 0.0f;
    int have = 0; /* fallback if inventory API absent in unit harness */
    int needed = 0;
    int total_recipes = rogue_craft_recipe_count();
    for (int r = 0; r < total_recipes; r++)
    {
        const RogueCraftRecipe* rec = rogue_craft_recipe_at(r);
        if (!rec)
            continue;
        for (int i = 0; i < rec->input_count; i++)
        {
            if (rec->inputs[i].def_index == item_def_index)
            {
                needed += rec->inputs[i].quantity; /* naive aggregate */
            }
        }
    }
    int deficit = needed - have;
    if (deficit <= 0)
        return 0.0f;
    return (float) deficit / (float) (have + 1);
}

/**
 * @brief Compute a dynamic spawn scalar for an item based on scarcity.
 *
 * Returns a multiplicative boost in [0.75,1.35] where higher values
 * encourage spawning of scarce materials. The scalar is attenuated by
 * softcap pressure so that abundant items do not over-inflate supply.
 *
 * @param item_def_index Item definition index
 * @return Spawn multiplier
 */
float rogue_craft_dynamic_spawn_scalar(int item_def_index)
{
    float scarcity = rogue_craft_material_scarcity(item_def_index);
    /* Map scarcity to [1,1.35] with diminishing returns, clamp, then apply soft cap suppression. */
    float boost = 1.0f + 0.35f * (1.0f - 1.0f / (1.0f + scarcity));
    if (boost > 1.35f)
        boost = 1.35f;
    float pressure = rogue_craft_material_softcap_pressure(item_def_index);
    if (pressure > 0)
    {
        /* pressure reduces boost towards 0.75 baseline floor proportionally */
        float minBase = 0.75f;
        boost = minBase + (boost - minBase) * (1.0f - pressure);
    }
    if (boost < 0.75f)
        boost = 0.75f;
    return boost;
}

/**
 * @brief Compute softcap pressure for an item based on on-hand counts.
 *
 * Uses item rarity to compute a tier-threshold; when player 'have'
 * exceeds threshold pressure grows toward 1.0 and saturates, which
 * downstream systems can use to suppress additional spawns or
 * bonuses.
 *
 * @param item_def_index Item definition index
 * @return Pressure in [0,1]
 */
float rogue_craft_material_softcap_pressure(int item_def_index)
{
    if (item_def_index < 0)
        return 0.0f;
    const RogueItemDef* def = rogue_item_def_at(item_def_index);
    if (!def)
        return 0.0f;
    /* Use rarity as a proxy for tier (rarity 0..4 -> tier 1..5). */
    int tier = def->rarity + 1;
    if (tier < 1)
        tier = 1;
    int have = 0; /* inventory count fallback */
    /* threshold scales superlinearly with tier so high tier allowed fewer raw pieces before
     * pressure. */
    int threshold = 40 / tier;
    if (threshold < 5)
        threshold = 5;
    if (have <= threshold)
        return 0.0f;
    int over = have - threshold;
    /* Pressure saturates when over >= threshold*2 */
    float pressure = (float) over / (float) (threshold * 2);
    if (pressure > 1.0f)
        pressure = 1.0f;
    return pressure;
}

/* Enhanced value model: integrate material quality bias and rarity curvature. We delegate base
 * logic to existing econ_value then adjust. */
/**
 * @brief Compute an enhanced numeric value for an item.
 *
 * Combines the item's base_value with slot multipliers (weapon/armor),
 * rarity curvature, affix contribution, durability scaling and a
 * material quality bias factor. The model is intentionally simple and
 * deterministic for predictable analytics and tooling.
 *
 * @param def_index Item definition index
 * @param rarity Rarity step (0..n), used to compute multiplier
 * @param affix_power_raw Raw affix power contributing to value
 * @param durability_fraction Fractional durability in [0,1]
 * @param material_quality_bias Bias in [0,1] representing material quality
 * @return Integer estimated item value (>=1)
 */
int rogue_craft_enhanced_item_value(int def_index, int rarity, int affix_power_raw,
                                    float durability_fraction, float material_quality_bias)
{
    const RogueItemDef* def = rogue_item_def_at(def_index);
    if (!def)
        return 0;
    int base = def->base_value;
    if (base <= 0)
        base = 1;
    /* Simplified slot factor: weapons weigh higher, armor medium, others baseline */
    float slot_factor = 1.0f;
    if (def->category == ROGUE_ITEM_WEAPON)
        slot_factor = 1.4f;
    else if (def->category == ROGUE_ITEM_ARMOR)
        slot_factor = 1.2f;
    float rarity_mult = 1.0f + 0.4f * rarity;
    if (rarity_mult < 1.0f)
        rarity_mult = 1.0f; /* linear baseline */
    /* Curved rarity bonus: apply soft curve so each rarity step adds diminishing return above 1. */
    float curved_rarity = 1.0f + (rarity_mult - 1.0f) * 0.85f; /* 15% tempering */
    /* Affix normalization similar to econ_value: raw scaled into [0,1] approx by dividing by 1000.
     */
    float affix_norm = (float) affix_power_raw / 1000.0f;
    if (affix_norm > 2.0f)
        affix_norm = 2.0f; /* allow >1 for strong items but clamp */
    if (durability_fraction < 0)
        durability_fraction = 0;
    if (durability_fraction > 1)
        durability_fraction = 1;
    /* Material quality bias (0..1 typical) mapped to multiplier 1..1.25 using smoothstep. */
    if (material_quality_bias < 0)
        material_quality_bias = 0;
    if (material_quality_bias > 1)
        material_quality_bias = 1;
    float q = material_quality_bias;
    float q_smooth = q * q * (3 - 2 * q); /* smoothstep */
    float quality_mult = 1.0f + 0.25f * q_smooth;
    float value_f = (float) base * slot_factor * curved_rarity * (1.0f + affix_norm) *
                    (0.5f + 0.5f * durability_fraction) * quality_mult;
    int value_i = (int) (value_f + 0.5f);
    if (value_i < 1)
        value_i = 1;
    return value_i;
}
