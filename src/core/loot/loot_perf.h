/* Phase 17 Performance & Memory Optimizations */
#ifndef ROGUE_LOOT_PERF_H
#define ROGUE_LOOT_PERF_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueLootPerfMetrics
    {
        uint32_t affix_pool_acquires;
        uint32_t affix_pool_releases;
        uint32_t affix_pool_max_in_use;
        uint32_t affix_roll_calls;
        uint32_t affix_roll_simd_sums;
        uint32_t affix_roll_scalar_sums;
        uint32_t affix_roll_total_weights;
        /* Phase 17.3 profiling harness timing (nanoseconds aggregated) */
        unsigned long long
            weight_sum_time_ns; /* time spent inside weight summation (SIMD+scalar) */
        unsigned long long affix_roll_time_ns; /* time spent building & selecting affixes */
    } RogueLootPerfMetrics;

    void rogue_loot_perf_reset(void);
    void rogue_loot_perf_get(RogueLootPerfMetrics* out);
    int rogue_loot_perf_simd_enabled(void);
    int rogue_loot_perf_test_rolls(int loops);
    /* Profiling helpers (Phase 17.3) */
    void rogue_loot_perf_affix_roll_begin(void);
    void rogue_loot_perf_affix_roll_end(void);

#ifdef __cplusplus
}
#endif
#endif
