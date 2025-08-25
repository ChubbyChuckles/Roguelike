#include "equipment_enchant.h"
#include "../../game/stat_cache.h"
#include "../crafting/crafting_journal.h"
#include "../crafting/material_registry.h" /* for rogue_material_tier_by_item */
#include "../crafting/rng_streams.h"
#include "../inventory/inventory.h"
#include "../loot/loot_affixes.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include "../vendor/economy.h"
#include <stdlib.h>

/* Material IDs looked up lazily (cached after first resolution) */
static int g_enchant_mat_def = -1; /* enchant_orb */
static int g_reforge_mat_def = -1; /* reforge_hammer */

static void resolve_material_ids(void)
{
    if (g_enchant_mat_def < 0)
        g_enchant_mat_def = rogue_item_def_index("enchant_orb");
    if (g_reforge_mat_def < 0)
        g_reforge_mat_def = rogue_item_def_index("reforge_hammer");
}

/* Phase 6.3: Integrate material scaling
   Scaling model:
     - Base cost formula as before.
     - Apply a multiplier based on average catalyst tier (if defined) else fallback to rarity
   modulation.
     - Apply diminishing inflation based on historical enchant count (stubbed via econ hook
   placeholder returning 0 for now) so future integration can reduce runaway costs.
*/
static int g_cached_prefix_tier = -1; /* lazily resolve from material registry via associated
                                         catalyst item (imbue_prefix_stone) if available */
static int g_cached_suffix_tier = -1; /* not strictly needed here but reserved */
static int resolve_material_tier_for_item(const char* item_id)
{
    if (!item_id)
        return -1;
    return rogue_material_tier_by_item(item_id);
}
static float catalyst_tier_multiplier(void)
{
    if (g_cached_prefix_tier < 0)
    {
        g_cached_prefix_tier = resolve_material_tier_for_item("imbue_prefix_stone");
    }
    int tier = g_cached_prefix_tier;
    if (tier < 0)
        return 1.0f; /* unknown -> neutral */
    /* Tier 0 ->1.00, Tier 5 ->1.10, Tier 10 ->1.20 scaling linear 0.02 per 1 tier beyond 0 capped
     */
    float mult = 1.0f + (tier * 0.02f);
    if (mult > 1.25f)
        mult = 1.25f;
    return mult;
}
static int enchant_cost_formula(int item_level, int rarity, int slots)
{
    if (item_level < 1)
        item_level = 1;
    if (rarity < 0)
        rarity = 0;
    if (rarity > 4)
        rarity = 4;
    if (slots < 0)
        slots = 0;
    int base = 50 + item_level * 5 + (rarity * rarity) * 25 + 10 * slots;
    float mult = catalyst_tier_multiplier();
    int cost = (int) (base * mult);
    if (cost < base)
        cost = base;
    return cost;
}
static int reforge_cost_formula(int item_level, int rarity, int slots)
{
    int base = enchant_cost_formula(item_level, rarity, slots);
    /* Reforge heavier base factor */
    int cost = base * 2;
    return cost;
}

static void reroll_affix(int* index_field, int* value_field, RogueAffixType type, int rarity,
                         unsigned int* rng, int avoid_index)
{
    if (!index_field || !value_field)
        return;
    *index_field = -1;
    *value_field = 0;
    /* Try a few times to avoid rerolling to the same affix index */
    int attempts = 0;
    int idx = -1;
    do
    {
        idx = rogue_affix_roll(type, rarity, rng);
        attempts++;
    } while (idx == avoid_index && attempts < 8);
    if (idx >= 0)
    {
        *index_field = idx;
        *value_field = rogue_affix_roll_value(idx, rng);
    }
}

int rogue_item_instance_enchant(int inst_index, int reroll_prefix, int reroll_suffix, int* out_cost)
{
    const RogueItemInstance* itc = rogue_item_instance_at(inst_index);
    if (!itc)
        return -1;
    const RogueItemDef* d = rogue_item_def_at(itc->def_index);
    if (!d)
        return -1;
    if (!reroll_prefix && !reroll_suffix)
        return -2;
    int has_prefix = itc->prefix_index >= 0;
    int has_suffix = itc->suffix_index >= 0;
    if (reroll_prefix && !has_prefix && reroll_suffix && !has_suffix)
        return -2; /* neither present */
    /* Respect protective locks: if locked, ignore reroll request for that affix */
    if (itc->prefix_locked)
        reroll_prefix = 0;
    if (itc->suffix_locked)
        reroll_suffix = 0;
    if (!reroll_prefix && !reroll_suffix)
        return -2; /* after lock filtering nothing to do */
    unsigned int rng = (unsigned int) (inst_index * 2654435761u) ^ (unsigned int) itc->item_level ^
                       0xBEEF1234u; /* deterministic seed basis */
    int cost = enchant_cost_formula(itc->item_level, itc->rarity, itc->socket_count);
    resolve_material_ids();
    int need_mat = (reroll_prefix && reroll_suffix) ? 1 : 0;
#ifdef ROGUE_TEST_DISABLE_CATALYSTS
    need_mat = 0;
#endif
    if (rogue_econ_gold() < cost)
        return -3;
    if (need_mat)
    {
        if (g_enchant_mat_def < 0 || rogue_inventory_get_count(g_enchant_mat_def) <= 0)
            return -4;
    }
    /* Deduct resources */
    rogue_econ_add_gold(-cost);
    if (need_mat)
        rogue_inventory_consume(g_enchant_mat_def, 1);
    /* Copy to mutable */
    RogueItemInstance* it = (RogueItemInstance*) itc;
    if (reroll_prefix && has_prefix)
    {
        int old = it->prefix_index;
        reroll_affix(&it->prefix_index, &it->prefix_value, ROGUE_AFFIX_PREFIX, it->rarity, &rng,
                     old);
    }
    if (reroll_suffix && has_suffix)
    {
        int old = it->suffix_index;
        reroll_affix(&it->suffix_index, &it->suffix_value, ROGUE_AFFIX_SUFFIX, it->rarity, &rng,
                     old);
    }
    /* Budget validation */
    if (rogue_item_instance_validate_budget(inst_index) != 0)
        return -5; /* Should not happen; treat as failure */
    rogue_stat_cache_mark_dirty();
    if (out_cost)
        *out_cost = cost; /* journal entry */
    unsigned int outcome_hash = (unsigned int) (it->prefix_index * 1315423911u) ^
                                (unsigned int) (it->suffix_index * 2654435761u) ^
                                (unsigned int) cost;
    rogue_craft_journal_append((unsigned int) inst_index, 0, 0, ROGUE_RNG_STREAM_ENHANCEMENT,
                               outcome_hash);
    return 0;
}

int rogue_item_instance_reforge(int inst_index, int* out_cost)
{
    const RogueItemInstance* itc = rogue_item_instance_at(inst_index);
    if (!itc)
        return -1;
    const RogueItemDef* d = rogue_item_def_at(itc->def_index);
    if (!d)
        return -1;
    unsigned int rng = (unsigned int) (inst_index * 11400714819323198485ull) ^
                       (unsigned int) itc->item_level ^ 0xC0FFEEU;
    resolve_material_ids();
    int cost = reforge_cost_formula(itc->item_level, itc->rarity, itc->socket_count);
    if (rogue_econ_gold() < cost)
        return -3;
    int can_consume_reforge = 1;
#ifndef ROGUE_TEST_DISABLE_CATALYSTS
    if (g_reforge_mat_def < 0 || rogue_inventory_get_count(g_reforge_mat_def) <= 0)
        return -6;
#else
    if (g_reforge_mat_def < 0 || rogue_inventory_get_count(g_reforge_mat_def) <= 0)
        can_consume_reforge = 0;
#endif
    /* Deduct */
    rogue_econ_add_gold(-cost);
    if (can_consume_reforge)
        rogue_inventory_consume(g_reforge_mat_def, 1);
    RogueItemInstance* it = (RogueItemInstance*) itc;
    /* Wipe affixes */
    it->prefix_index = it->suffix_index = -1;
    it->prefix_value = it->suffix_value = 0;
    /* Rarity determines how many affixes to roll similar to original generation (reuse logic) */
    int rarity = it->rarity;
    /* Basic mimic of generation rules */
    if (rarity >= 2)
    {
        if (rarity >= 3)
        {
            reroll_affix(&it->prefix_index, &it->prefix_value, ROGUE_AFFIX_PREFIX, rarity, &rng,
                         -1);
            reroll_affix(&it->suffix_index, &it->suffix_value, ROGUE_AFFIX_SUFFIX, rarity, &rng,
                         -1);
        }
        else
        {
            int choose_prefix = (rng & 1) == 0;
            if (choose_prefix)
                reroll_affix(&it->prefix_index, &it->prefix_value, ROGUE_AFFIX_PREFIX, rarity, &rng,
                             -1);
            else
                reroll_affix(&it->suffix_index, &it->suffix_value, ROGUE_AFFIX_SUFFIX, rarity, &rng,
                             -1);
        }
    }
    /* Clear existing gem sockets content (preserve count) */
    for (int s = 0; s < it->socket_count && s < 6; ++s)
    {
        it->sockets[s] = -1;
    }
    if (rogue_item_instance_validate_budget(inst_index) != 0)
        return -5;
    rogue_stat_cache_mark_dirty();
    if (out_cost)
        *out_cost = cost;
    unsigned int outcome_hash = (unsigned int) (it->prefix_index * 109951u) ^
                                (unsigned int) (it->suffix_index * 334214467u) ^
                                (unsigned int) cost;
    rogue_craft_journal_append((unsigned int) inst_index, 0, 0, ROGUE_RNG_STREAM_ENHANCEMENT,
                               outcome_hash);
    return 0;
}

/* Protective seal implementation */
static int g_seal_mat_def = -1; /* protective_seal */
int rogue_item_instance_apply_protective_seal(int inst_index, int lock_prefix, int lock_suffix)
{
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    if (!lock_prefix && !lock_suffix)
        return -2;
    int need_prefix = lock_prefix && it->prefix_index >= 0 && !it->prefix_locked;
    int need_suffix = lock_suffix && it->suffix_index >= 0 && !it->suffix_locked;
    if (!need_prefix && !need_suffix)
        return -2;
    if (g_seal_mat_def < 0)
        g_seal_mat_def = rogue_item_def_index("protective_seal");
    if (g_seal_mat_def < 0 || rogue_inventory_get_count(g_seal_mat_def) <= 0)
        return -3;
    rogue_inventory_consume(g_seal_mat_def, 1);
    if (need_prefix)
        it->prefix_locked = 1;
    if (need_suffix)
        it->suffix_locked = 1;
    return 0;
}
int rogue_item_instance_is_prefix_locked(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    return it ? it->prefix_locked : 0;
}
int rogue_item_instance_is_suffix_locked(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    return it ? it->suffix_locked : 0;
}
