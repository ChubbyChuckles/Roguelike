/* Phase 2.2 JSON Integration: Validate affix budget calculations with JSON item implicit stats
   - Loads a temporary JSON weapon definition
   - Spawns an instance and generates affixes (high rarity to encourage rolls)
   - Asserts generated total affix weight <= computed budget
   - If at least one affix rolled, artificially exceeds budget via apply_affixes and
     asserts validator reports over-budget (<0) */
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

static int write_file(const char* path, const char* contents)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0)
        f = NULL;
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return 0;
    fwrite(contents, 1, strlen(contents), f);
    fclose(f);
    return 1;
}

int main(void)
{
    const char* path = "tmp_affix_budget_items.json";
    const char* json =
        "[\n"
        " {\n"
        "  \"id\":\"budget_sword\",\n"
        "  \"name\":\"Budget Sword\",\n"
        "  \"category\":2,\n" /* weapon category to enable damage/attribute affix gating */
        "  \"level_req\":1,\n"
        "  \"stack_max\":1,\n"
        "  \"base_value\":10,\n"
        "  \"base_damage_min\":2,\n"
        "  \"base_damage_max\":4,\n"
        "  \"base_armor\":0,\n"
        "  \"sprite_sheet\":\"sheet.png\",\n"
        "  \"sprite_tx\":0,\n"
        "  \"sprite_ty\":0,\n"
        "  \"sprite_tw\":1,\n"
        "  \"sprite_th\":1,\n"
        "  \"rarity\":1,\n"
        "  \"flags\":0\n"
        " }\n"
        "]\n";

    if (!write_file(path, json))
    {
        fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL write\n");
        return 1;
    }

    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_json(path);
    if (added <= 0)
    {
        fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL load added=%d\n", added);
        remove(path);
        return 2;
    }
    int def = rogue_item_def_index("budget_sword");
    if (def < 0)
    {
        fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL def index\n");
        remove(path);
        return 3;
    }

    rogue_items_init_runtime();
    /* Load real affixes so generation rolls meaningful stats */
    rogue_affixes_reset();
    int aload = rogue_affixes_load_from_cfg("assets/affixes.cfg");
    if (aload <= 0)
    {
        fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL affix_load=%d\n", aload);
        remove(path);
        return 10;
    }

    int inst = rogue_items_spawn(def, 1, 0.f, 0.f);
    if (inst < 0)
    {
        fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL spawn\n");
        remove(path);
        return 4;
    }

    unsigned int rng = 123456789u;
    /* Use max rarity (3) parameter to raise chance of at least one affix */
    (void) rogue_item_instance_generate_affixes(inst, &rng, 3);

    int weight = rogue_item_instance_total_affix_weight(inst);
    const RogueItemInstance* in = rogue_item_instance_at(inst);
    if (!in)
    {
        fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL missing instance\n");
        remove(path);
        return 5;
    }
    int max_budget = rogue_budget_max(in->item_level, in->rarity);
    if (max_budget <= 0)
    {
        fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL max_budget=%d\n", max_budget);
        remove(path);
        return 6;
    }
    if (weight > max_budget)
    {
        fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL weight=%d budget=%d\n", weight, max_budget);
        remove(path);
        return 7;
    }

    if (in->prefix_index >= 0 || in->suffix_index >= 0)
    {
        int new_prefix_val = in->prefix_value;
        int new_suffix_val = in->suffix_value;
        /* Force one affix value over budget */
        if (in->prefix_index >= 0)
            new_prefix_val = max_budget + 10; /* exceed */
        else if (in->suffix_index >= 0)
            new_suffix_val = max_budget + 10;
        if (rogue_item_instance_apply_affixes(inst, in->rarity, in->prefix_index, new_prefix_val,
                                              in->suffix_index, new_suffix_val) != 0)
        {
            fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL apply_affixes\n");
            remove(path);
            return 8;
        }
        int v = rogue_item_instance_validate_budget(inst);
        if (v >= 0)
        {
            fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL expected over budget validate=%d\n", v);
            remove(path);
            return 9;
        }
    }
    else
    {
        /* Deterministically attach a valid prefix so we can still exercise over-budget path. */
        const RogueItemDef* base_def = rogue_item_def_at(in->def_index);
        int candidate = -1;
        for (int i = 0; i < rogue_affix_count(); ++i)
        {
            const RogueAffixDef* a = rogue_affix_at(i);
            if (!a || a->type != ROGUE_AFFIX_PREFIX)
                continue;
            int allowed = 0;
            if (a->stat == ROGUE_AFFIX_STAT_DAMAGE_FLAT)
                allowed = (base_def && base_def->category == ROGUE_ITEM_WEAPON);
            else if (a->stat == ROGUE_AFFIX_STAT_AGILITY_FLAT)
                allowed = (base_def && (base_def->category == ROGUE_ITEM_WEAPON ||
                                        base_def->category == ROGUE_ITEM_ARMOR ||
                                        base_def->category == ROGUE_ITEM_GEM));
            else if (a->stat == ROGUE_AFFIX_STAT_NONE)
                allowed = 1;
            if (!allowed)
                continue;
            candidate = i;
            break;
        }
        if (candidate >= 0)
        {
            if (rogue_item_instance_apply_affixes(inst, in->rarity, candidate, 1, -1, 0) != 0)
            {
                fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL forced_attach\n");
                remove(path);
                return 11;
            }
            weight = rogue_item_instance_total_affix_weight(inst);
            if (weight > max_budget)
            {
                fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL forced_weight=%d budget=%d\n", weight,
                        max_budget);
                remove(path);
                return 12;
            }
            /* Now force over-budget */
            if (rogue_item_instance_apply_affixes(inst, in->rarity, candidate, max_budget + 10, -1,
                                                  0) != 0)
            {
                fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL forced_over_budget_apply\n");
                remove(path);
                return 13;
            }
            int v = rogue_item_instance_validate_budget(inst);
            if (v >= 0)
            {
                fprintf(stderr, "AFFIX_BUDGET_JSON_FAIL expected over budget forced_validate=%d\n",
                        v);
                remove(path);
                return 14;
            }
            /* Restore within budget for final success report (not strictly required) */
            if (rogue_item_instance_apply_affixes(inst, in->rarity, candidate, 1, -1, 0) == 0)
            {
                weight = rogue_item_instance_total_affix_weight(inst);
            }
            printf("AFFIX_BUDGET_JSON_NOTE deterministic_attach weight=%d budget=%d\n", weight,
                   max_budget);
        }
        else
        {
            /* No suitable affix found; still validated base case */
            printf("AFFIX_BUDGET_JSON_NOTE no_affix_roll weight=%d budget=%d\n", weight,
                   max_budget);
        }
    }

    remove(path);
    printf("AFFIX_BUDGET_JSON_OK weight=%d budget=%d\n", weight, max_budget);
    return 0;
}
