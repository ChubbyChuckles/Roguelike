#include "core/crafting_skill.h"
#include "core/crafting.h"
#include "core/crafting_queue.h"
#include <string.h>

#ifndef ROGUE_CRAFT_DISC_XP_CAP
#define ROGUE_CRAFT_DISC_XP_CAP 1000000
#endif

static int g_xp[ROGUE_CRAFT_DISC__COUNT];
static unsigned int g_discovered_bits[64]; /* supports up to 64*32=2048 recipes */

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
int rogue_craft_skill_xp(enum RogueCraftDiscipline disc)
{
    if (disc < 0 || disc >= ROGUE_CRAFT_DISC__COUNT)
        return 0;
    return g_xp[disc];
}
int rogue_craft_skill_level(enum RogueCraftDiscipline disc)
{
    return level_from_xp(rogue_craft_skill_xp(disc));
}
int rogue_craft_skill_xp_to_next(enum RogueCraftDiscipline disc)
{
    return xp_to_next_from(rogue_craft_skill_xp(disc));
}

/* Simple perk scaling tiers */
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

void rogue_craft_discovery_reset(void) { memset(g_discovered_bits, 0, sizeof g_discovered_bits); }
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
int rogue_craft_recipe_is_discovered(int recipe_index)
{
    int w;
    unsigned int m;
    if (!bit_index(recipe_index, &w, &m))
        return 0;
    return (g_discovered_bits[w] & m) != 0;
}
void rogue_craft_recipe_mark_discovered(int recipe_index)
{
    int w;
    unsigned int m;
    if (!bit_index(recipe_index, &w, &m))
        return;
    g_discovered_bits[w] |= m;
}

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
