#/***************************************************************
 * @file crafting_automation.c
 * @brief Crafting & Gathering Phase 9 - Automation & Smart Assist
 *
 * Collection of heuristics and helper utilities used by the
 * crafting and gathering tooling: plan requirement expansion,
 * gather route scoring, refining suggestions, salvage vs craft
 * decision heuristics and idle recommendations.
 *
 * All functions are pure helpers and query existing registries
 * (recipes, materials, inventory) without mutating global state.
 */
#include "crafting_automation.h"
#include "../inventory/inventory.h"
#include "../loot/loot_item_defs.h"
#include "../vendor/econ_materials.h"
#include "../vendor/salvage.h"
#include "crafting.h"
#include "crafting_queue.h"
#include "gathering.h"
#include "material_refine.h"
#include "material_registry.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Comparison helper for sorting RogueCraftPlanReq by item_def.
 *
 * Used to provide a stable ordering when producing requirement lists.
 *
 * @param a Pointer to first RogueCraftPlanReq
 * @param b Pointer to second RogueCraftPlanReq
 * @return negative if a<b, zero if equal, positive if a>b
 */
static int cmp_plan(const void* a, const void* b)
{
    const RogueCraftPlanReq* A = (const RogueCraftPlanReq*) a;
    const RogueCraftPlanReq* B = (const RogueCraftPlanReq*) b;
    return (A->item_def - B->item_def);
}
/**
 * @brief Comparison helper for gather node priority sorting.
 *
 * Expects arrays of two floats where index 1 is the priority/score.
 * Sorts descending by score.
 *
 * @param a Pointer to float[2]
 * @param b Pointer to float[2]
 * @return 1 if a<b (lower priority), -1 if a>b, 0 if equal
 */
static int cmp_node_priority(const void* a, const void* b)
{
    const float* A = (const float*) a;
    const float* B = (const float*) b;
    if (A[1] < B[1])
        return 1;
    if (A[1] > B[1])
        return -1;
    return 0;
}
/**
 * @brief Deterministic comparator for RogueRefineSuggestion entries.
 *
 * Orders primarily by material_def then by from_quality to create a
 * predictable suggestion ordering.
 *
 * @param a Pointer to first RogueRefineSuggestion
 * @param b Pointer to second RogueRefineSuggestion
 * @return negative if a<b, zero if equal, positive if a>b
 */
static int cmp_refine(const void* a, const void* b)
{
    const RogueRefineSuggestion* A = (const RogueRefineSuggestion*) a;
    const RogueRefineSuggestion* B = (const RogueRefineSuggestion*) b;
    if (A->material_def != B->material_def)
        return A->material_def - B->material_def;
    return A->from_quality - B->from_quality;
}

/* Helper: accumulate requirement into dynamic array */
/**
 * @brief Accumulate a requirement into a dynamic plan array.
 *
 * If the item_def already exists in arr it increments its required
 * quantity. If not, and there is capacity, it appends a new entry.
 * Invalid item_def or non-positive qty are ignored.
 *
 * @param arr Array of RogueCraftPlanReq to mutate
 * @param count Pointer to current count (updated on append)
 * @param max Maximum capacity of arr
 * @param item_def Item definition index to accumulate
 * @param qty Quantity to add
 */
static void plan_accum(RogueCraftPlanReq* arr, int* count, int max, int item_def, int qty)
{
    if (item_def < 0 || qty <= 0)
        return;
    for (int i = 0; i < *count; i++)
    {
        if (arr[i].item_def == item_def)
        {
            arr[i].required += qty;
            return;
        }
    }
    if (*count >= max)
        return;
    arr[*count].item_def = item_def;
    arr[*count].required = qty;
    arr[*count].have = 0;
    arr[*count].missing = 0;
    (*count)++;
}

/**
 * @brief Check whether a recipe can be expanded into inputs.
 *
 * A recipe is considered expandable if it is non-NULL and has at
 * least one input. This is a small helper used during recursive
 * expansion of crafting requirements.
 *
 * @param r Pointer to recipe
 * @return 1 if expandable, 0 otherwise
 */
static int recipe_is_input_expandable(const RogueCraftRecipe* r)
{
    if (!r)
        return 0;
    return r->input_count > 0;
}

/**
 * @brief Find a recipe that produces the specified item_def.
 *
 * Scans all registered craft recipes and returns the first recipe
 * whose output_def matches item_def. Returns NULL when none found.
 *
 * @param item_def Item definition index to search for
 * @return Pointer to matching RogueCraftRecipe or NULL
 */
static const RogueCraftRecipe* find_recipe_producing(int item_def)
{
    int rc = rogue_craft_recipe_count();
    for (int i = 0; i < rc; i++)
    {
        const RogueCraftRecipe* r = rogue_craft_recipe_at(i);
        if (r && r->output_def == item_def)
            return r;
    }
    return NULL;
}

/**
 * @brief Build a requirement plan for producing a recipe output.
 *
 * This function computes the list of input item definitions and
 * quantities required to produce `batch_qty` outputs of `recipe`.
 * When `recursive` is non-zero the function will attempt to expand
 * inputs that can themselves be crafted using available recipes up
 * to `max_depth` levels.
 *
 * The computed array `out` is populated with entries containing
 * required, have (current inventory count) and missing quantities.
 * The function returns the number of entries written to `out`, or
 * a negative error code on invalid arguments.
 *
 * @param recipe Target recipe to plan
 * @param batch_qty Number of recipe outputs desired (uses 1 if <=0)
 * @param recursive Non-zero to enable recipe expansion
 * @param max_depth Maximum expansion depth (per recursive flag)
 * @param out Preallocated output array of RogueCraftPlanReq
 * @param max_out Capacity of out array
 * @return Number of unique item_def requirements written, or -1 on error
 */
int rogue_craft_plan_requirements(const RogueCraftRecipe* recipe, int batch_qty, int recursive,
                                  int max_depth, RogueCraftPlanReq* out, int max_out)
{
    if (!recipe || !out || max_out <= 0)
        return -1;
    if (batch_qty <= 0)
        batch_qty = 1;
    int raw_count = 0; /* direct inputs */
    for (int i = 0; i < recipe->input_count; i++)
    {
        plan_accum(out, &raw_count, max_out, recipe->inputs[i].def_index,
                   recipe->inputs[i].quantity * batch_qty);
    }
    if (recursive && max_depth > 0)
    { /* one level expansion per depth */
        for (int depth = 0; depth < max_depth; depth++)
        {
            int added_in_pass = 0;
            for (int i = 0; i < raw_count && raw_count < max_out; i++)
            {
                const RogueCraftPlanReq cur =
                    out[i]; /* attempt expansion if a recipe produces this input */
                const RogueCraftRecipe* prod = find_recipe_producing(cur.item_def);
                if (!prod || !recipe_is_input_expandable(prod))
                    continue;                      /* expand */
                int needed_missing = cur.required; /* naive expansion ignore have until later */
                /* remove current entry by swapping last (safe as order sorted later) */
                out[i] = out[raw_count - 1];
                raw_count--;
                i--; /* reprocess swapped index */
                for (int k = 0; k < prod->input_count; k++)
                {
                    plan_accum(out, &raw_count, max_out, prod->inputs[k].def_index,
                               prod->inputs[k].quantity * needed_missing);
                }
                added_in_pass = 1;
            }
            if (!added_in_pass)
                break;
        }
    }
    /* fill have/missing */
    for (int i = 0; i < raw_count; i++)
    {
        int have = rogue_inventory_get_count(out[i].item_def);
        out[i].have = have;
        int miss = out[i].required - have;
        if (miss < 0)
            miss = 0;
        out[i].missing = miss;
    }
    /* sort */
    for (int i = 0; i < raw_count - 1; i++)
    {
        for (int j = i + 1; j < raw_count; j++)
        {
            if (out[j].item_def < out[i].item_def)
            {
                RogueCraftPlanReq tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
    return raw_count;
}

/**
 * @brief Suggest gather node targets ranked by scarcity coverage.
 *
 * Computes a simple scarcity-driven coverage score for each
 * `RogueGatherNodeDef` and returns a sorted list of node indices.
 * The returned node indices reference the gather definition pool and
 * are ordered by descending expected usefulness for gathering.
 *
 * @param out_node_defs Preallocated int array to receive node indices
 * @param max_out Capacity of out_node_defs
 * @return Number of node indices written, 0 if none, or -1 on error
 */
int rogue_craft_gather_route(int* out_node_defs, int max_out)
{
    if (!out_node_defs || max_out <= 0)
        return -1;
    int rc = rogue_craft_recipe_count();
    if (rc <= 0)
        return 0; /* compute scarcity per material (by item_def) */
    /* For simplicity scarcity over item defs referenced in recipes */
    float scores[ROGUE_GATHER_NODE_CAP][2];
    int scount = 0;
    int ndc = rogue_gather_def_count();
    if (ndc <= 0)
        return 0;
    for (int nd = 0; nd < ndc && nd < ROGUE_GATHER_NODE_CAP; nd++)
    {
        const RogueGatherNodeDef* d = rogue_gather_def_at(nd);
        if (!d || d->material_count <= 0)
            continue; /* compute coverage score */
        double total_w = 0.0;
        for (int m = 0; m < d->material_count; m++)
            total_w += (double) (d->material_weights[m] > 0 ? d->material_weights[m] : 0);
        if (total_w <= 0)
            continue;
        double coverage = 0.0;
        for (int m = 0; m < d->material_count; m++)
        {
            int mat_def = d->material_defs[m];
            if (mat_def < 0)
                continue;
            const RogueMaterialDef* md = rogue_material_get(mat_def);
            if (!md)
                continue;
            int item_def = md->item_def_index;
            int have = rogue_inventory_get_count(item_def);
            long needed = 0;
            for (int r = 0; r < rc; r++)
            {
                const RogueCraftRecipe* cr = rogue_craft_recipe_at(r);
                for (int ii = 0; ii < cr->input_count; ii++)
                {
                    if (cr->inputs[ii].def_index == item_def)
                        needed += cr->inputs[ii].quantity;
                }
            }
            if (needed <= 0)
                continue;
            double missing = (double) (needed - have);
            if (missing < 0)
                missing = 0;
            double scarcity_ratio = missing / ((double) have + 1.0);
            double weight_frac = (double) d->material_weights[m] / total_w;
            coverage += scarcity_ratio * weight_frac;
        }
        if (coverage <= 0.0)
            continue;
        scores[scount][0] = (float) nd;
        scores[scount][1] = (float) coverage;
        scount++;
    }
    if (scount == 0)
        return 0; /* simple insertion sort by coverage desc */
    for (int i = 0; i < scount - 1; i++)
    {
        for (int j = i + 1; j < scount; j++)
        {
            if (scores[j][1] > scores[i][1])
            {
                float t0 = scores[i][0], t1 = scores[i][1];
                scores[i][0] = scores[j][0];
                scores[i][1] = scores[j][1];
                scores[j][0] = t0;
                scores[j][1] = t1;
            }
        }
    }
    int outc = scount < max_out ? scount : max_out;
    for (int i = 0; i < outc; i++)
        out_node_defs[i] = (int) scores[i][0];
    return outc;
}

/**
 * @brief Generate material refine suggestions.
 *
 * Scans material qualities and suggests refinement operations where
 * lower-quality buckets have sufficient quantity to be consumed into
 * a higher-quality bucket. The suggestion contains conservative
 * consume counts and estimated produced counts for the target
 * quality.
 *
 * @param avg_threshold Skip materials whose average quality >= threshold
 * @param min_bucket Minimum items in a quality bucket before suggestion
 * @param delta_q Quality delta to apply when refining (defaults to 1)
 * @param out Preallocated output suggestion array
 * @param max_out Capacity of out
 * @return Number of suggestions written, or -1 on invalid args
 */
int rogue_craft_refine_suggestions(int avg_threshold, int min_bucket, int delta_q,
                                   RogueRefineSuggestion* out, int max_out)
{
    if (!out || max_out <= 0)
        return -1;
    if (delta_q <= 0)
        delta_q = 1;
    int count = 0;
    int mc = rogue_material_count();
    for (int m = 0; m < mc && count < max_out; m++)
    {
        const RogueMaterialDef* md = rogue_material_get(m);
        if (!md)
            continue;
        int avg = rogue_material_quality_average(m);
        if (avg >= 0 && avg >= avg_threshold)
            continue; /* scan buckets */
        for (int q = 0; q < ROGUE_MATERIAL_QUALITY_MAX && count < max_out; q += 5)
        {
            int have = rogue_material_quality_count(m, q);
            if (have >= min_bucket)
            {
                RogueRefineSuggestion* s = &out[count++];
                s->material_def = m;
                s->from_quality = q;
                int toq = q + delta_q;
                if (toq > ROGUE_MATERIAL_QUALITY_MAX)
                    toq = ROGUE_MATERIAL_QUALITY_MAX;
                s->to_quality = toq;
                s->consume_count = have / 2;
                if (s->consume_count <= 0)
                {
                    count--;
                    continue;
                } /* estimate production EV = consume * 0.70 */
                s->est_produced = (int) ((long long) s->consume_count * 70ll / 100ll);
            }
        }
    }
    /* sort deterministic */
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = i + 1; j < count; j++)
        {
            if (out[j].material_def < out[i].material_def ||
                (out[j].material_def == out[i].material_def &&
                 out[j].from_quality < out[i].from_quality))
            {
                RogueRefineSuggestion tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
    return count;
}

/**
 * @brief Decide whether salvaging or crafting yields better value.
 *
 * Heuristically computes an approximate salvage monetary value for
 * the specified item and the expected net gain from crafting using
 * `upgrade_recipe`. Returns 1 to prefer crafting when net gain >
 * salvage_value, otherwise 0. Returns negative on error.
 *
 * Note: this does not modify inventory, it only estimates values.
 *
 * @param item_def_index Item definition index to evaluate
 * @param item_rarity Optional rarity override (-1 to use def rarity)
 * @param upgrade_recipe Recipe used to upgrade/craft the item
 * @param out_salvage_value Optional out param for salvage estimate
 * @param out_craft_net_gain Optional out param for craft net gain
 * @return 1 if craft is preferred, 0 if salvage preferred, negative on error
 */
int rogue_craft_decision_salvage_vs_craft(int item_def_index, int item_rarity,
                                          const RogueCraftRecipe* upgrade_recipe,
                                          double* out_salvage_value, double* out_craft_net_gain)
{
    if (!upgrade_recipe)
        return -1;
    if (item_def_index < 0)
        return -1; /* salvage value: replicate salvage_item logic without modifying inventory */
    const RogueItemDef* d = rogue_item_def_at(item_def_index);
    if (!d)
        return -1;
    int rarity = item_rarity >= 0
                     ? item_rarity
                     : d->rarity; /* salvage yields quantity of material, approximate value */
    /* approximate salvage yield: 1<<rarity scaled by base value bracket (replicate salvage.c
     * formula) */
    int scale = (d->base_value > 150) ? 3 : (d->base_value > 50 ? 2 : 1);
    int salvage_units =
        (1 << (rarity < 5 ? rarity : 4)) * scale; /* find material def representative */
    int mat_def = rogue_item_def_index(rarity < 3 ? "arcane_dust" : "primal_shard");
    int mat_val = 0;
    if (mat_def >= 0)
        mat_val = rogue_econ_material_base_value(mat_def);
    if (mat_val <= 0)
        mat_val = d->base_value > 0 ? d->base_value : 10;
    double salvage_value = (double) salvage_units * (double) mat_val;
    /* craft cost value: sum inputs missing * their base values; output gain: base value difference
     * * expected improvement factor (rarity delta) */
    double craft_cost = 0.0;
    for (int i = 0; i < upgrade_recipe->input_count; i++)
    {
        int id = upgrade_recipe->inputs[i].def_index;
        const RogueItemDef* di = rogue_item_def_at(id);
        if (!di)
            continue;
        int have = rogue_inventory_get_count(id);
        int need = upgrade_recipe->inputs[i].quantity;
        int miss = need - have;
        if (miss < 0)
            miss = 0;
        int unit_val = di->base_value > 0 ? di->base_value : 10;
        craft_cost += (double) miss * (double) unit_val;
    }
    /* net gain: (upgrade output base value - current base value) - craft_cost */
    const RogueItemDef* outd = rogue_item_def_at(upgrade_recipe->output_def);
    double out_val = outd ? (double) (outd->base_value > 0 ? outd->base_value : d->base_value)
                          : (double) d->base_value;
    double cur_val = (double) (d->base_value > 0 ? d->base_value : 10);
    double net_gain = (out_val - cur_val) - craft_cost;
    if (out_salvage_value)
        *out_salvage_value = salvage_value;
    if (out_craft_net_gain)
        *out_craft_net_gain = net_gain;
    return net_gain > salvage_value ? 1 : 0;
}

/**
 * @brief Recommend a material to idle-gather based on scarcity.
 *
 * Computes a simple score across all recipe inputs and returns the
 * item_def or mapped material_def that has the highest scarcity
 * ratio (missing / (have + 1)). If a material mapping exists, the
 * material_def is returned; otherwise the raw item_def is returned.
 *
 * @return material_def index, item_def index, or -1 if no recommendation
 */
int rogue_craft_idle_recommend_material(void)
{
    int rc = rogue_craft_recipe_count();
    if (rc <= 0)
        return -1;
    double best_score = 0.0;
    int best_item_def = -1; /* evaluate over all recipe inputs (item defs) */
    for (int r = 0; r < rc; r++)
    {
        const RogueCraftRecipe* cr = rogue_craft_recipe_at(r);
        if (!cr)
            continue;
        for (int i = 0; i < cr->input_count; i++)
        {
            int item_def = cr->inputs[i].def_index;
            if (item_def < 0)
                continue;
            long needed = 0;
            for (int rr = 0; rr < rc; rr++)
            {
                const RogueCraftRecipe* cr2 = rogue_craft_recipe_at(rr);
                for (int ii = 0; ii < cr2->input_count; ii++)
                {
                    if (cr2->inputs[ii].def_index == item_def)
                        needed += cr2->inputs[ii].quantity;
                }
            }
            if (needed <= 0)
                continue;
            int have = rogue_inventory_get_count(item_def);
            double missing = (double) (needed - have);
            if (missing <= 0)
                continue;
            double score = missing / ((double) have + 1.0);
            if (score > best_score)
            {
                best_score = score;
                best_item_def = item_def;
            }
        }
    }
    if (best_item_def < 0)
        return -1; /* map back to material_def if possible */
    const RogueMaterialDef* md = rogue_material_find_by_item(best_item_def);
    if (md)
    {
        return (int) (md - rogue_material_get(0));
    }
    return best_item_def;
}
