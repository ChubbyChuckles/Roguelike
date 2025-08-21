/* Test rolling window rarity statistics (6.3) */
#include "core/loot/loot_stats.h"
#include <stdio.h>

static int fail(const char* m){ fprintf(stderr,"FAIL:%s\n", m); return 1; }

int main(void){
    rogue_loot_stats_reset();
    int seq[] = {0,0,1,2,4,3,4,4,2,1,0,4};
    int n = (int)(sizeof(seq)/sizeof(seq[0]));
    for(int i=0;i<n;i++) rogue_loot_stats_record_rarity(seq[i]);
    int counts[5]; rogue_loot_stats_snapshot(counts);
    if(counts[0]!=3 || counts[1]!=2 || counts[2]!=2 || counts[3]!=1 || counts[4]!=4) return fail("counts_initial");
    /* Overwrite ring fully to test eviction */
    for(int i=0;i<ROGUE_LOOT_STATS_CAP; i++) rogue_loot_stats_record_rarity(4);
    int after[5]; rogue_loot_stats_snapshot(after);
    if(after[4] != ROGUE_LOOT_STATS_CAP) return fail("overwrite_fill");
    if(after[0]!=0||after[1]!=0||after[2]!=0||after[3]!=0) return fail("evict");
    printf("LOOT_STATS_WINDOW_OK cap=%d final4=%d\n", ROGUE_LOOT_STATS_CAP, after[4]);
    return 0;
}
