#include "equipment_gems.h"
#include "../inventory/inventory.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include "../stat_cache.h"
#include "../vendor/economy.h"
#include "equipment.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ROGUE_GEM_DEF_CAP
#define ROGUE_GEM_DEF_CAP 128
#endif

static RogueGemDef g_gems[ROGUE_GEM_DEF_CAP];
static int g_gem_count = 0;

int rogue_gem_register(const RogueGemDef* def)
{
    if (!def || !def->id[0])
        return -1;
    if (g_gem_count >= ROGUE_GEM_DEF_CAP)
        return -2;
    g_gems[g_gem_count] = *def;
    return g_gem_count++;
}
const RogueGemDef* rogue_gem_at(int index)
{
    if (index < 0 || index >= g_gem_count)
        return NULL;
    return &g_gems[index];
}
int rogue_gem_find_by_item_def(int item_def_index)
{
    for (int i = 0; i < g_gem_count; i++)
        if (g_gems[i].item_def_index == item_def_index)
            return i;
    return -1;
}
int rogue_gem_count(void) { return g_gem_count; }

int rogue_gem_socket_cost(const RogueGemDef* g)
{
    if (!g)
        return 0;
    int sum = g->strength + g->dexterity + g->vitality + g->intelligence + g->armor_flat +
              g->resist_physical + g->resist_fire + g->resist_cold + g->resist_lightning +
              g->resist_poison + g->resist_status + g->pct_strength + g->pct_dexterity +
              g->pct_vitality + g->pct_intelligence;
    if (sum < 0)
        sum = 0;
    return 10 + sum * 2;
}

int rogue_item_instance_socket_insert_pay(int inst_index, int slot, int gem_def_index,
                                          int* out_cost)
{
    const RogueGemDef* g = rogue_gem_at(gem_def_index);
    if (!g)
        return -4;
    int cost = rogue_gem_socket_cost(g);
    if (rogue_econ_gold() < cost)
        return -6;
    int r = rogue_item_instance_socket_insert(inst_index, slot, g->item_def_index);
    if (r == 0)
    {
        rogue_econ_add_gold(-cost);
        if (out_cost)
            *out_cost = cost;
    }
    return r;
}
int rogue_item_instance_socket_remove_refund(int inst_index, int slot, int return_to_inventory)
{
    int item_def = rogue_item_instance_get_socket(inst_index, slot);
    if (item_def < 0)
        return -3;
    int gem_index = rogue_gem_find_by_item_def(item_def);
    const RogueGemDef* g = rogue_gem_at(gem_index);
    int refund = g ? (rogue_gem_socket_cost(g) / 2) : 0;
    int r = rogue_item_instance_socket_remove(inst_index, slot);
    if (r == 0)
    {
        if (refund > 0)
            rogue_econ_add_gold(refund);
        if (return_to_inventory && item_def >= 0)
            rogue_inventory_add(item_def, 1);
    }
    return r;
}

/* Simple CSV loader for gem defs */
int rogue_gem_defs_load_from_cfg(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return -1;
    char line[512];
    int added = 0;
    while (fgets(line, sizeof line, f))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        char work[512];
        size_t len = strlen(line);
        if (len >= sizeof(work))
            len = sizeof(work) - 1;
        memcpy(work, line, len);
        work[len] = '\0';
        char* cur = work;
        char* fields[24];
        int nf = 0;
        while (cur && nf < 24)
        {
            char* start = cur;
            while (*cur && *cur != ',')
            {
                cur++;
            }
            if (*cur == ',')
            {
                *cur = '\0';
                cur++;
            }
            else if (*cur == '\0')
            {
                cur = NULL;
            }
            fields[nf++] = start;
        }
        if (nf < 18)
            continue;
        RogueGemDef d;
        memset(&d, 0, sizeof d);
        {
            size_t idlen = strlen(fields[0]);
            if (idlen >= sizeof(d.id))
                idlen = sizeof(d.id) - 1;
            memcpy(d.id, fields[0], idlen);
            d.id[idlen] = '\0';
        }
        d.item_def_index = rogue_item_def_index(fields[1]);
        d.strength = atoi(fields[2]);
        d.dexterity = atoi(fields[3]);
        d.vitality = atoi(fields[4]);
        d.intelligence = atoi(fields[5]);
        d.armor_flat = atoi(fields[6]);
        d.resist_physical = atoi(fields[7]);
        d.resist_fire = atoi(fields[8]);
        d.resist_cold = atoi(fields[9]);
        d.resist_lightning = atoi(fields[10]);
        d.resist_poison = atoi(fields[11]);
        d.resist_status = atoi(fields[12]);
        d.pct_strength = atoi(fields[13]);
        d.pct_dexterity = atoi(fields[14]);
        d.pct_vitality = atoi(fields[15]);
        d.pct_intelligence = atoi(fields[16]);
        d.proc_chance = atoi(fields[17]);
        if (nf > 18)
            d.conditional_flags = atoi(fields[18]);
        if (d.item_def_index >= 0)
        {
            if (rogue_gem_register(&d) >= 0)
                added++;
        }
    }
    fclose(f);
    return added;
}

/* Aggregation helper (called from equipment_stats) */
void rogue_gems_aggregate_equipped(void)
{
    int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
    int r_phys = 0, r_fire = 0, r_cold = 0, r_light = 0, r_poison = 0, r_status = 0;
    int pct_str = 0, pct_dex = 0, pct_vit = 0, pct_int = 0;
    for (int slot = 0; slot < ROGUE_EQUIP__COUNT; ++slot)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) slot);
        if (inst < 0)
            continue;
        int sc = rogue_item_instance_socket_count(inst);
        if (sc <= 0)
            continue;
        for (int s = 0; s < sc; s++)
        {
            int gem_item_def = rogue_item_instance_get_socket(inst, s);
            if (gem_item_def < 0)
                continue;
            int gidx = rogue_gem_find_by_item_def(gem_item_def);
            if (gidx < 0)
                continue;
            const RogueGemDef* g = rogue_gem_at(gidx);
            str += g->strength;
            dex += g->dexterity;
            vit += g->vitality;
            intel += g->intelligence;
            armor += g->armor_flat;
            r_phys += g->resist_physical;
            r_fire += g->resist_fire;
            r_cold += g->resist_cold;
            r_light += g->resist_lightning;
            r_poison += g->resist_poison;
            r_status += g->resist_status;
            pct_str += g->pct_strength;
            pct_dex += g->pct_dexterity;
            pct_vit += g->pct_vitality;
            pct_int += g->pct_intelligence;
        }
    }
    g_player_stat_cache.affix_strength += str;
    g_player_stat_cache.affix_dexterity += dex;
    g_player_stat_cache.affix_vitality += vit;
    g_player_stat_cache.affix_intelligence += intel;
    g_player_stat_cache.affix_armor_flat += armor;
    g_player_stat_cache.resist_physical += r_phys;
    g_player_stat_cache.resist_fire += r_fire;
    g_player_stat_cache.resist_cold += r_cold;
    g_player_stat_cache.resist_lightning += r_light;
    g_player_stat_cache.resist_poison += r_poison;
    g_player_stat_cache.resist_status += r_status; /* Percent bonuses (approx flat conversion) */
    if (pct_str > 0)
        g_player_stat_cache.affix_strength += (g_player_stat_cache.base_strength * pct_str) / 100;
    if (pct_dex > 0)
        g_player_stat_cache.affix_dexterity += (g_player_stat_cache.base_dexterity * pct_dex) / 100;
    if (pct_vit > 0)
        g_player_stat_cache.affix_vitality += (g_player_stat_cache.base_vitality * pct_vit) / 100;
    if (pct_int > 0)
        g_player_stat_cache.affix_intelligence +=
            (g_player_stat_cache.base_intelligence * pct_int) / 100;
}
