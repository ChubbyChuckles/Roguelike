/* Phase 14: Performance & Memory Optimization Implementation */
#include "core/equipment/equipment_perf.h"
#include "core/equipment/equipment.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* SoA buffers */
int g_equip_slot_strength[ROGUE_EQUIP__COUNT];
int g_equip_slot_dexterity[ROGUE_EQUIP__COUNT];
int g_equip_slot_vitality[ROGUE_EQUIP__COUNT];
int g_equip_slot_intelligence[ROGUE_EQUIP__COUNT];
int g_equip_slot_armor[ROGUE_EQUIP__COUNT];

int g_equip_total_strength = 0;
int g_equip_total_dexterity = 0;
int g_equip_total_vitality = 0;
int g_equip_total_intelligence = 0;
int g_equip_total_armor = 0;

/* Simple linear arena (single frame). */
#define EQUIP_FRAME_ARENA_CAP 8192
static unsigned char g_frame_arena[EQUIP_FRAME_ARENA_CAP];
static size_t g_frame_off = 0, g_frame_high = 0;
void* rogue_equip_frame_alloc(size_t size, size_t align)
{
    if (align < 1)
        align = 1;
    size_t mask = align - 1;
    size_t aligned = (g_frame_off + mask) & ~mask;
    if (aligned + size > EQUIP_FRAME_ARENA_CAP)
        return NULL;
    void* p = g_frame_arena + aligned;
    g_frame_off = aligned + size;
    if (g_frame_off > g_frame_high)
        g_frame_high = g_frame_off;
    return p;
}
void rogue_equip_frame_reset(void) { g_frame_off = 0; }
size_t rogue_equip_frame_high_water(void) { return g_frame_high; }
size_t rogue_equip_frame_capacity(void) { return EQUIP_FRAME_ARENA_CAP; }

/* Minimal profiler (string hash to bucket). */
#define PROF_ZONE_CAP 16
typedef struct
{
    char name[24];
    double total_ms;
    int count;
    int used;
    clock_t begin_clock;
    int active;
} Zone;
static Zone g_zones[PROF_ZONE_CAP];
void rogue_equip_profiler_reset(void) { memset(g_zones, 0, sizeof g_zones); }
static Zone* find_zone(const char* name)
{
    int free_idx = -1;
    for (int i = 0; i < PROF_ZONE_CAP; i++)
    {
        if (g_zones[i].used)
        {
            if (strncmp(g_zones[i].name, name, sizeof g_zones[i].name) == 0)
                return &g_zones[i];
        }
        else if (free_idx < 0)
            free_idx = i;
    }
    if (free_idx >= 0)
    {
        Zone* z = &g_zones[free_idx];
        z->used = 1; /* safe copy */
        size_t len = strlen(name);
        if (len >= sizeof z->name)
            len = sizeof z->name - 1;
        memcpy(z->name, name, len);
        z->name[len] = '\0';
        return z;
    }
    return NULL;
}
void rogue_equip_profiler_zone_begin(const char* name)
{
    Zone* z = find_zone(name);
    if (!z || z->active)
        return;
    z->begin_clock = clock();
    z->active = 1;
}
void rogue_equip_profiler_zone_end(const char* name)
{
    Zone* z = find_zone(name);
    if (!z || !z->active)
        return;
    clock_t end = clock();
    double ms = 1000.0 * (double) (end - z->begin_clock) / (double) CLOCKS_PER_SEC;
    z->total_ms += ms;
    z->count++;
    z->active = 0;
}
int rogue_equip_profiler_zone_stats(const char* name, double* total_ms, int* count)
{
    Zone* z = find_zone(name);
    if (!z || z->count == 0)
        return -1;
    if (total_ms)
        *total_ms = z->total_ms;
    if (count)
        *count = z->count;
    return 0;
}
int rogue_equip_profiler_dump(char* buf, int cap)
{
    if (!buf || cap < 4)
        return -1;
    int off = 0;
    int n = snprintf(buf + off, cap - off, "{");
    if (n < 0 || n >= cap - off)
        return -1;
    off += n;
    for (int i = 0; i < PROF_ZONE_CAP; i++)
    {
        Zone* z = &g_zones[i];
        if (!z->used || z->count == 0)
            continue;
        n = snprintf(buf + off, cap - off, "\"%s\":{\"ms\":%.3f,\"count\":%d},", z->name,
                     z->total_ms, z->count);
        if (n < 0 || n >= cap - off)
            return -1;
        off += n;
    }
    if (off > 1 && buf[off - 1] == ',')
        off--;
    n = snprintf(buf + off, cap - off, "}");
    if (n < 0 || n >= cap - off)
        return -1;
    off += n;
    if (off < cap)
        buf[off] = '\0';
    return off;
}

/* Scalar aggregation fallback */
static void aggregate_scalar(void)
{
    g_equip_total_strength = g_equip_total_dexterity = g_equip_total_vitality =
        g_equip_total_intelligence = g_equip_total_armor = 0;
    for (int s = 0; s < ROGUE_EQUIP__COUNT; s++)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) s);
        int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
        if (inst >= 0)
        {
            const RogueItemInstance* it = rogue_item_instance_at(inst);
            if (it)
            {
                const RogueItemDef* d = rogue_item_def_at(it->def_index);
                if (d)
                {
                    armor = d->base_armor; /* simplistic primary stats derived from rarity for
                                              illustration */
                    str = d->rarity;
                    dex = d->rarity;
                    vit = d->rarity;
                    intel = d->rarity;
                }
            }
        }
        g_equip_slot_strength[s] = str;
        g_equip_slot_dexterity[s] = dex;
        g_equip_slot_vitality[s] = vit;
        g_equip_slot_intelligence[s] = intel;
        g_equip_slot_armor[s] = armor;
        g_equip_total_strength += str;
        g_equip_total_dexterity += dex;
        g_equip_total_vitality += vit;
        g_equip_total_intelligence += intel;
        g_equip_total_armor += armor;
    }
}

/* Pseudo-SIMD aggregation: batch process 4 slots at a time (no actual intrinsics to keep
 * portability). */
static void aggregate_simd_like(void)
{
    g_equip_total_strength = g_equip_total_dexterity = g_equip_total_vitality =
        g_equip_total_intelligence = g_equip_total_armor = 0;
    int s = 0;
    for (; s + 3 < ROGUE_EQUIP__COUNT; s += 4)
    {
        for (int i = 0; i < 4; i++)
        {
            int slot = s + i;
            int inst = rogue_equip_get((enum RogueEquipSlot) slot);
            int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
            if (inst >= 0)
            {
                const RogueItemInstance* it = rogue_item_instance_at(inst);
                if (it)
                {
                    const RogueItemDef* d = rogue_item_def_at(it->def_index);
                    if (d)
                    {
                        armor = d->base_armor;
                        str = d->rarity;
                        dex = d->rarity;
                        vit = d->rarity;
                        intel = d->rarity;
                    }
                }
            }
            g_equip_slot_strength[slot] = str;
            g_equip_slot_dexterity[slot] = dex;
            g_equip_slot_vitality[slot] = vit;
            g_equip_slot_intelligence[slot] = intel;
            g_equip_slot_armor[slot] = armor;
            g_equip_total_strength += str;
            g_equip_total_dexterity += dex;
            g_equip_total_vitality += vit;
            g_equip_total_intelligence += intel;
            g_equip_total_armor += armor;
        }
    }
    for (; s < ROGUE_EQUIP__COUNT; s++)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) s);
        int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
        if (inst >= 0)
        {
            const RogueItemInstance* it = rogue_item_instance_at(inst);
            if (it)
            {
                const RogueItemDef* d = rogue_item_def_at(it->def_index);
                if (d)
                {
                    armor = d->base_armor;
                    str = d->rarity;
                    dex = d->rarity;
                    vit = d->rarity;
                    intel = d->rarity;
                }
            }
        }
        g_equip_slot_strength[s] = str;
        g_equip_slot_dexterity[s] = dex;
        g_equip_slot_vitality[s] = vit;
        g_equip_slot_intelligence[s] = intel;
        g_equip_slot_armor[s] = armor;
        g_equip_total_strength += str;
        g_equip_total_dexterity += dex;
        g_equip_total_vitality += vit;
        g_equip_total_intelligence += intel;
        g_equip_total_armor += armor;
    }
}

void rogue_equipment_aggregate(enum RogueEquipAggregateMode mode)
{
    rogue_equip_profiler_zone_begin(mode == ROGUE_EQUIP_AGGREGATE_SIMD ? "agg_simd" : "agg_scalar");
    if (mode == ROGUE_EQUIP_AGGREGATE_SIMD)
        aggregate_simd_like();
    else
        aggregate_scalar();
    rogue_equip_profiler_zone_end(mode == ROGUE_EQUIP_AGGREGATE_SIMD ? "agg_simd" : "agg_scalar");
}
