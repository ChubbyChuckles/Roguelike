/* Phase 17 Performance & Memory Optimizations basic test */
#include "core/loot_perf.h"
#include <stdio.h>

static int fail=0;
#define CHECK(c,msg) do { \
    if(!(c)) { \
        printf("FAIL:%s line %d %s\n",__FILE__,__LINE__,msg); \
        fail=1; \
    } \
} while(0)

int main(void){ setvbuf(stdout,NULL,_IONBF,0); rogue_loot_perf_reset();
    int loops=64; int ok = rogue_loot_perf_test_rolls(loops); CHECK(ok==loops, "all_rolls_success");
    RogueLootPerfMetrics m; rogue_loot_perf_get(&m);
    CHECK(m.affix_pool_acquires== (unsigned)loops, "acquires_match");
    CHECK(m.affix_pool_releases== (unsigned)loops, "releases_match");
    CHECK(m.affix_pool_max_in_use<=loops && m.affix_pool_max_in_use>0, "max_in_use_bounds");
    CHECK(m.affix_roll_calls==(unsigned)loops, "roll_calls_match");
    CHECK(m.affix_roll_total_weights>0, "weights_accumulated");
    if(fail){ printf("FAILURES\n"); return 1; } printf("OK:loot_phase17_perf\n"); return 0; }
