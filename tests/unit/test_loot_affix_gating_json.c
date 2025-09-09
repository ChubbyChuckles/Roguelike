/* Validates affix gating with JSON-loaded base items: weapon-only affixes never roll for armor. */
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>

extern int rogue_generation_gated_affix_roll(RogueAffixType type, int rarity,
                                             unsigned int* rng_state, const RogueItemDef* base_def,
                                             int existing_prefix, int existing_suffix);

static int load_items_and_affixes(void)
{
    const char* item_dirs[] = {"assets/items", "../assets/items", "../../assets/items", NULL};
    const char* affix_paths[] = {"assets/affixes.cfg", "../assets/affixes.cfg",
                                 "../../assets/affixes.cfg", NULL};
    rogue_item_defs_reset();
    int icount = 0;
    for (int i = 0; item_dirs[i]; i++)
    {
        icount = rogue_item_defs_load_directory_json(item_dirs[i]);
        if (icount > 0)
            break;
    }
    if (icount <= 0)
        return -1; /* item load fail */
    rogue_affixes_reset();
    int aload = 0;
    for (int i = 0; affix_paths[i]; i++)
    {
        aload = rogue_affixes_load_from_cfg(affix_paths[i]);
        if (aload > 0)
            break;
    }
    if (aload <= 0)
        return -2; /* affix load fail */
    return icount;
}

int main(void)
{
    if (load_items_and_affixes() < 0)
    {
        printf("AFFIX_GATING_JSON_FAIL load\n");
        return 10;
    }
    /* Runtime after successful definition & affix load */
    rogue_items_init_runtime();
    int armor_index = rogue_item_def_index("armor_leather_cap");
    int weapon_index = rogue_item_def_index("weapon_iron_sword");
    if (armor_index < 0 || weapon_index < 0)
    {
        printf("AFFIX_GATING_JSON_FAIL base_defs\n");
        return 11;
    }
    const RogueItemDef* armor = rogue_item_def_at(armor_index);
    const RogueItemDef* weapon = rogue_item_def_at(weapon_index);
    unsigned int rng = 1337u;
    int armor_rolls = 0;
    int armor_illegal = 0;
    /* Use higher rarity (3) to mirror legacy gating test behavior; rarity 0 produced no rolls for
     * armor. */
    for (int i = 0; i < 200; i++)
    {
        /* Try prefix */
        int aidx = rogue_generation_gated_affix_roll(ROGUE_AFFIX_PREFIX, 3, &rng, armor, -1, -1);
        if (aidx >= 0)
        {
            const RogueAffixDef* a = rogue_affix_at(aidx);
            if (a)
            {
                armor_rolls++;
                if (a->stat == ROGUE_AFFIX_STAT_DAMAGE_FLAT)
                    armor_illegal++;
            }
        }
        /* Try suffix */
        int sidx = rogue_generation_gated_affix_roll(ROGUE_AFFIX_SUFFIX, 3, &rng, armor, -1, -1);
        if (sidx >= 0)
        {
            const RogueAffixDef* s = rogue_affix_at(sidx);
            if (s)
            {
                armor_rolls++;
                if (s->stat == ROGUE_AFFIX_STAT_DAMAGE_FLAT)
                    armor_illegal++;
            }
        }
    }
    if (armor_illegal != 0)
    {
        printf("AFFIX_GATING_JSON_FAIL armor_illegal=%d rolls=%d\n", armor_illegal, armor_rolls);
        return 12;
    }
    int weapon_hits = 0;
    rng = 424242u;
    for (int i = 0; i < 200 && weapon_hits == 0; i++)
    {
        int aidx = rogue_generation_gated_affix_roll(ROGUE_AFFIX_PREFIX, 3, &rng, weapon, -1, -1);
        if (aidx >= 0)
        {
            const RogueAffixDef* a = rogue_affix_at(aidx);
            if (a && a->stat == ROGUE_AFFIX_STAT_DAMAGE_FLAT)
                weapon_hits++;
        }
        int sidx = rogue_generation_gated_affix_roll(ROGUE_AFFIX_SUFFIX, 3, &rng, weapon, -1, -1);
        if (sidx >= 0)
        {
            const RogueAffixDef* s = rogue_affix_at(sidx);
            if (s && s->stat == ROGUE_AFFIX_STAT_DAMAGE_FLAT)
                weapon_hits++;
        }
    }
    if (weapon_hits == 0)
    {
        printf("AFFIX_GATING_JSON_FAIL weapon_no_damage_flat\n");
        return 13;
    }
    printf("AFFIX_GATING_JSON_OK armor_rolls=%d\n", armor_rolls);
    return 0;
}
