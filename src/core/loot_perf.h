/* Phase 17 Performance & Memory Optimizations */
#ifndef ROGUE_LOOT_PERF_H
#define ROGUE_LOOT_PERF_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RogueLootPerfMetrics {
    uint32_t affix_pool_acquires;
    uint32_t affix_pool_releases;
    uint32_t affix_pool_max_in_use;
    uint32_t affix_roll_calls;
    uint32_t affix_roll_simd_sums;
    uint32_t affix_roll_scalar_sums;
    uint32_t affix_roll_total_weights;
} RogueLootPerfMetrics;

void rogue_loot_perf_reset(void);
void rogue_loot_perf_get(RogueLootPerfMetrics* out);
int  rogue_loot_perf_simd_enabled(void);
int  rogue_loot_perf_test_rolls(int loops);

#ifdef __cplusplus
}
#endif
#endif
