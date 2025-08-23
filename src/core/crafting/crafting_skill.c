/**
 * @file crafting_skill.c
 * @brief Crafting skill progression, perks, and discovery handling.
 *
 * Provides XP bookkeeping for crafting disciplines, perk-derived
 * modifiers (material cost, speed, duplicate chance, quality floor), and
 * the simple recipe discovery system used by the crafting queue.
 */

#include "core/crafting/crafting_skill.h"
#include "core/crafting/crafting.h"
#include "core/crafting/crafting_queue.h"
#include <string.h>

#ifndef ROGUE_CRAFT_DISC_XP_CAP
/**
 * @brief Maximum XP allowed for any crafting discipline.
 *
 * Defined as a compile-time cap to avoid integer overflow and provide a
 * stable progression ceiling.
 */
#define ROGUE_CRAFT_DISC_XP_CAP 1000000
#endif

/** @brief XP counters indexed by @c RogueCraftDiscipline. */
static int g_xp[ROGUE_CRAFT_DISC__COUNT];

/**
 * @brief Bitset of discovered recipes.
 *
 * Each unsigned int stores 32 recipe discovered flags. The array size of 64
 * supports up to 2048 recipe entries.
 */
static unsigned int g_discovered_bits[64]; /* supports up to 64*32=2048 recipes */

/**
 * @brief Convert total XP to a crafting skill level.
 *
 * Uses a smooth quadratic-ish progression where xp needed for level L is
 * approximated by 50*L*(L+1). The function returns the integer level for
 * the provided XP value.
 *
 * @param xp Total experience points for a discipline.
 * @return Computed level (>= 0).
 */
static int level_from_xp(int xp)
{
    /* Smooth quadratic-ish curve: L^2 * 50 progression: xp_needed(L)=50*L*(L+1) */
    int level = 0;
    int need = 50;
    int accum = 0;
    while (level < 200)
    {
        if (accum + need > xp)
            break;
        accum += need;
        level++;
        need = 50 * (level + 1);
    }
    return level;
}

/**
 * @brief Return XP needed to reach the next level from a given XP value.
 *
 * @param xp Current total XP.
 * @return Milliseconds of XP remaining until next level.
 */
static int xp_to_next_from(int xp)
{
    int lvl = level_from_xp(xp);
    int accum = 0;
    for (int l = 0; l < lvl; l++)
    {
        accum += 50 * (l + 1);
    }
    int next_need = 50 * (lvl + 1);
    return (accum + next_need) - xp;
}

/**
 * @brief Reset all crafting discipline XP and discovery state.
 *
 * Clears per-discipline XP counters and the discovered recipe bitset.
 */
void rogue_craft_skill_reset(void)
{
    for (int i = 0; i < ROGUE_CRAFT_DISC__COUNT; i++)
        g_xp[i] = 0;
    memset(g_discovered_bits, 0, sizeof g_discovered_bits);
}
void rogue_craft_skill_gain(enum RogueCraftDiscipline disc, int xp)
{
    if (disc < 0 || disc >= ROGUE_CRAFT_DISC__COUNT || xp <= 0)
        return;
    long long v = (long long) g_xp[disc] + xp;
    if (v > ROGUE_CRAFT_DISC_XP_CAP)
        v = ROGUE_CRAFT_DISC_XP_CAP;
    g_xp[disc] = (int) v;
}
/**
 * @brief Get current XP for a crafting discipline.
 *
 * @param disc Discipline enum value.
 * @return XP amount, or 0 for invalid discipline.
 */
int rogue_craft_skill_xp(enum RogueCraftDiscipline disc)
{
    if (disc < 0 || disc >= ROGUE_CRAFT_DISC__COUNT)
        return 0;
    return g_xp[disc];
}

/**
 * @brief Compute the crafting skill level from current XP for a discipline.
 *
 * @param disc Discipline enum value.
 * @return Level (>= 0).
 */
int rogue_craft_skill_level(enum RogueCraftDiscipline disc)
{
    return level_from_xp(rogue_craft_skill_xp(disc));
}

/**
 * @brief XP remaining until the next level for a discipline.
 *
 * @param disc Discipline enum value.
 * @return XP remaining to next level, or 0 for invalid discipline.
 */
int rogue_craft_skill_xp_to_next(enum RogueCraftDiscipline disc)
{
    return xp_to_next_from(rogue_craft_skill_xp(disc));
}

/* Simple perk scaling tiers */
/**
 * @brief Resolve perk-derived tiers from a skill level.
 *
 * Populates the provided output pointers with percentages/bonuses based on
 * the tier table. Callers may pass non-NULL pointers for the values they
 * need.
 *
 * @param lvl Skill level.
 * @param cost_pct Out: material cost percent (100 = no reduction).
 * @param speed_pct Out: time/speed percent (100 = base speed).
 * @param dup_pct Out: duplicate chance percent.
 * @param qfloor Out: quality floor bonus.
 */
static void perk_levels(int lvl, int* cost_pct, int* speed_pct, int* dup_pct, int* qfloor)
{
    /* Baseline 100% cost/time, 0 dup, 0 qfloor */
    *cost_pct = 100;
    *speed_pct = 100;
    *dup_pct = 0;
    *qfloor = 0;
    if (lvl >= 5)
    {
        *cost_pct = 95;
        *speed_pct = 95;
    }
    if (lvl >= 10)
    {
        *cost_pct = 92;
        *speed_pct = 90;
        *dup_pct = 2;
    }
    if (lvl >= 20)
    {
        *cost_pct = 88;
        *speed_pct = 85;
        *dup_pct = 5;
        *qfloor = 5;
    }
    if (lvl >= 30)
    {
        *cost_pct = 85;
        *speed_pct = 80;
        *dup_pct = 8;
        *qfloor = 8;
    }
    if (lvl >= 40)
    {
        *cost_pct = 82;
        *speed_pct = 75;
        *dup_pct = 12;
        *qfloor = 10;
    }
    if (lvl >= 50)
    {
        *cost_pct = 80;
        *speed_pct = 70;
        *dup_pct = 15;
        *qfloor = 12;
    }
}
int rogue_craft_perk_material_cost_pct(enum RogueCraftDiscipline disc)
{
    int l = rogue_craft_skill_level(disc);
    int c, s, d, q;
    perk_levels(l, &c, &s, &d, &q);
    return c;
}
int rogue_craft_perk_speed_pct(enum RogueCraftDiscipline disc)
{
    int l = rogue_craft_skill_level(disc);
    int c, s, d, q;
    perk_levels(l, &c, &s, &d, &q);
    return s;
}
int rogue_craft_perk_duplicate_chance_pct(enum RogueCraftDiscipline disc)
{
    int l = rogue_craft_skill_level(disc);
    int c, s, d, q;
    perk_levels(l, &c, &s, &d, &q);
    return d;
}
int rogue_craft_quality_floor_bonus(enum RogueCraftDiscipline disc)
{
    int l = rogue_craft_skill_level(disc);
    int c, s, d, q;
    perk_levels(l, &c, &s, &d, &q);
    return q;
}

/**
 * @brief Clear the discovered recipe bitset.
 */
void rogue_craft_discovery_reset(void) { memset(g_discovered_bits, 0, sizeof g_discovered_bits); }

/**
 * @brief Helper to compute word index and bit mask for a recipe index.
 *
 * @param recipe_index Recipe numeric index (>= 0).
 * @param word Out parameter receiving the word index (may be NULL).
 * @param mask Out parameter receiving the bit mask for that word (may be NULL).
 * @return 1 on success, 0 if the recipe index is invalid or out of supported range.
 */
static int bit_index(int recipe_index, int* word, unsigned int* mask)
{
    if (recipe_index < 0)
        return 0;
    int w = recipe_index / 32;
    int b = recipe_index % 32;
    if (w >= (int) (sizeof(g_discovered_bits) / sizeof(g_discovered_bits[0])))
        return 0;
    if (word)
        *word = w;
    if (mask)
        *mask = (1u << b);
    return 1;
}
/**
 * @brief Query whether a recipe has been discovered.
 *
 * @param recipe_index Numeric recipe index.
 * @return Non-zero if discovered, zero otherwise.
 */
int rogue_craft_recipe_is_discovered(int recipe_index)
{
    int w;
    unsigned int m;
    if (!bit_index(recipe_index, &w, &m))
        return 0;
    return (g_discovered_bits[w] & m) != 0;
}

/**
 * @brief Mark a recipe as discovered in the bitset.
 *
 * No-op for invalid recipe indices.
 */
void rogue_craft_recipe_mark_discovered(int recipe_index)
{
    int w;
    unsigned int m;
    if (!bit_index(recipe_index, &w, &m))
        return;
    g_discovered_bits[w] |= m;
}

/**
 * @brief Unlock discovery of recipes that use the crafted item's output.
 *
 * When an item is crafted, any recipe that lists that item's definition
 * as one of its inputs becomes discoverable and is marked discovered.
 * This implements a simple dependency-based discovery mechanism.
 *
 * @param crafted_recipe_index Index of the recipe that was just crafted.
 */
void rogue_craft_discovery_unlock_dependencies(int crafted_recipe_index)
{
    const RogueCraftRecipe* r = rogue_craft_recipe_at(crafted_recipe_index);
    if (!r)
        return;
    int output = r->output_def;
    int total = rogue_craft_recipe_count();
    for (int i = 0; i < total; i++)
    {
        if (rogue_craft_recipe_is_discovered(i))
            continue;
        const RogueCraftRecipe* cand = rogue_craft_recipe_at(i);
        if (!cand)
            continue;
        for (int k = 0; k < cand->input_count; k++)
        {
            if (cand->inputs[k].def_index == output)
            {
                rogue_craft_recipe_mark_discovered(i);
                break;
            }
        }
    }
}

/**
 * @brief Map a station id to its crafting discipline.
 *
 * Provides the discipline that corresponds to a given station id. Defaults
 * to smithing for unknown station ids.
 *
 * @param station_id Station id from @c RogueCraftStation enum.
 * @return Corresponding @c RogueCraftDiscipline value.
 */
enum RogueCraftDiscipline rogue_craft_station_discipline(int station_id)
{
    switch (station_id)
    {
    case ROGUE_CRAFT_STATION_FORGE:
        return ROGUE_CRAFT_DISC_SMITHING;
    case ROGUE_CRAFT_STATION_WORKBENCH:
        return ROGUE_CRAFT_DISC_SMITHING;
    case ROGUE_CRAFT_STATION_ALCHEMY:
        return ROGUE_CRAFT_DISC_ALCHEMY;
    case ROGUE_CRAFT_STATION_ALTAR:
        return ROGUE_CRAFT_DISC_ENCHANTING;
    default:
        return ROGUE_CRAFT_DISC_SMITHING;
    }
}
