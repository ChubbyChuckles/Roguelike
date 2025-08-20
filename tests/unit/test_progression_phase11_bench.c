/* Phase 11.3 Micro-benchmark: measure selective vs full recompute cost (synthetic) */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "core/stat_cache.h"
#include "entities/player.h"

static RoguePlayer make_player(void){ RoguePlayer p; memset(&p,0,sizeof(p)); p.strength=50; p.dexterity=40; p.vitality=35; p.intelligence=25; p.crit_rating=400; p.haste_rating=300; p.avoidance_rating=150; p.crit_chance=20; p.crit_damage=175; p.max_health=500; return p; }

static double now_ms(void){ clock_t c = clock(); return (double)c * 1000.0 / (double)CLOCKS_PER_SEC; }

int main(void){
    RoguePlayer p = make_player();
    for(int i=0;i<5;i++){ rogue_stat_cache_mark_dirty(); rogue_stat_cache_update(&p); }
    const int ITERS=4000;
    double t_full_start=now_ms();
    for(int i=0;i<ITERS;i++){ rogue_stat_cache_mark_dirty(); rogue_stat_cache_update(&p); }
    double t_full = now_ms()-t_full_start;
    double t_buff_start=now_ms();
    for(int i=0;i<ITERS;i++){ rogue_stat_cache_mark_buff_dirty(); rogue_stat_cache_update(&p); }
    double t_buff = now_ms()-t_buff_start;
    if(t_full > 0 && t_buff > 0){ double ratio = t_full / t_buff; assert(ratio > 1.05 || (t_full < 5.0)); }
    printf("progression_phase11_bench: full=%.2fms buff=%.2fms\n", t_full, t_buff);
    return 0;
}
