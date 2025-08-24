#include "loot_affixes.h"
#include "loot_generation.h"
#include "loot_item_defs.h"

/* Internal helper previously static inside monolithic loot_generation.c now isolated for module
   split (Cleanup 24.4). Exposed to core translation unit via extern declaration (not in public
   header). */
int rogue_generation_gated_affix_roll(RogueAffixType type, int rarity, unsigned int* rng_state,
                                      const RogueItemDef* base_def, int existing_prefix,
                                      int existing_suffix)
{
    int indices[ROGUE_MAX_AFFIXES];
    int weights[ROGUE_MAX_AFFIXES];
    int count = 0;
    int total = 0;
    int acount = rogue_affix_count();
    for (int i = 0; i < acount; i++)
    {
        const RogueAffixDef* a = rogue_affix_at(i);
        if (!a)
            continue;
        if (a->type != type)
            continue;
        /* Affix gating rules (8.2) duplicated here to keep core file lean */
        int allowed = 0;
        if (base_def && a)
        {
            switch (a->stat)
            {
            case ROGUE_AFFIX_STAT_DAMAGE_FLAT:
                allowed = (base_def->category == ROGUE_ITEM_WEAPON);
                break;
            case ROGUE_AFFIX_STAT_AGILITY_FLAT:
                allowed = (base_def->category == ROGUE_ITEM_WEAPON ||
                           base_def->category == ROGUE_ITEM_ARMOR ||
                           base_def->category == ROGUE_ITEM_GEM);
                break;
            case ROGUE_AFFIX_STAT_NONE:
                allowed = 1;
                break;
            default:
                allowed = 0;
                break;
            }
        }
        if (!allowed)
            continue;
        if (i == existing_prefix || i == existing_suffix)
            continue; /* duplicate avoidance (8.6) */
        int w = a->weight_per_rarity[rarity];
        if (w <= 0)
            continue;
        indices[count] = i;
        weights[count] = w;
        total += w;
        count++;
        if (count >= ROGUE_MAX_AFFIXES)
            break;
    }
    if (total <= 0 || count == 0)
        return -1;
    *rng_state = (*rng_state * 1664525u) + 1013904223u;
    unsigned int pick = (*rng_state) % (unsigned) total;
    int acc = 0;
    for (int k = 0; k < count; k++)
    {
        acc += weights[k];
        if (pick < (unsigned) acc)
            return indices[k];
    }
    return indices[count - 1];
}
