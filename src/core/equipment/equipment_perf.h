/* Phase 14: Performance & Memory Optimization Helpers */
#ifndef ROGUE_EQUIPMENT_PERF_H
#define ROGUE_EQUIPMENT_PERF_H
#include "core/equipment/equipment.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif

    /* Structure-of-Arrays per-slot stat contributions (filled by aggregator). */
    extern int g_equip_slot_strength[ROGUE_EQUIP__COUNT];
    extern int g_equip_slot_dexterity[ROGUE_EQUIP__COUNT];
    extern int g_equip_slot_vitality[ROGUE_EQUIP__COUNT];
    extern int g_equip_slot_intelligence[ROGUE_EQUIP__COUNT];
    extern int g_equip_slot_armor[ROGUE_EQUIP__COUNT];

    /* Aggregated totals from last call (scalar or SIMD). */
    extern int g_equip_total_strength;
    extern int g_equip_total_dexterity;
    extern int g_equip_total_vitality;
    extern int g_equip_total_intelligence;
    extern int g_equip_total_armor;

    /* Arena (frame pool) for temporary allocations during aggregation / tests. */
    void* rogue_equip_frame_alloc(size_t size, size_t align);
    void rogue_equip_frame_reset(void);        /* resets arena for new frame */
    size_t rogue_equip_frame_high_water(void); /* peak usage (for tests) */
    size_t rogue_equip_frame_capacity(void);

    /* Aggregation variants */
    enum RogueEquipAggregateMode
    {
        ROGUE_EQUIP_AGGREGATE_SCALAR = 0,
        ROGUE_EQUIP_AGGREGATE_SIMD = 1
    };
    void rogue_equipment_aggregate(enum RogueEquipAggregateMode mode);

    /* Lightweight profiler zones (micro-profiler). */
    void rogue_equip_profiler_reset(void);
    void rogue_equip_profiler_zone_begin(const char* name);
    void rogue_equip_profiler_zone_end(const char* name);
    int rogue_equip_profiler_zone_stats(const char* name, double* total_ms,
                                        int* count); /* returns 0 on success */
    int rogue_equip_profiler_dump(char* buf, int cap);

#ifdef __cplusplus
}
#endif
#endif
