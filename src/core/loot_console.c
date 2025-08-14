#include <stdio.h>
#include <string.h>
#include <time.h>
#include "loot_console.h"
#include "loot_stats.h"
#include "loot_dynamic_weights.h"
#include "loot_rarity.h"

#define ROGUE_RARITY_MAX 5
static const char* rarity_names[ROGUE_RARITY_MAX] = {"COMMON","UNCOMMON","RARE","EPIC","LEGENDARY"};

int rogue_loot_histogram_format(char* out, int out_sz){
    if(!out || out_sz<=0) return -1;
    int counts[ROGUE_RARITY_MAX];
    rogue_loot_stats_snapshot(counts);
    int written = 0;
    for(int i=0;i<ROGUE_RARITY_MAX;i++){
        int c = counts[i];
        int n = snprintf(out+written, (size_t)(out_sz-written), "%s:%d\n", rarity_names[i], c);
        if(n<0) return -1;
        if(n >= out_sz-written){ if(out_sz>0) out[out_sz-1]='\0'; return written; }
        written += n;
    }
    int total=0; for(int i=0;i<ROGUE_RARITY_MAX;i++) total += counts[i];
    if(out_sz-written>0) snprintf(out+written, (size_t)(out_sz-written), "TOTAL:%d\n", total);
    return written;
}

int rogue_loot_export_telemetry(const char* path){
    if(!path) return -1;
    FILE* f = NULL; if(fopen_s(&f, path, "wb")!=0 || !f) return -2;
    int counts[ROGUE_RARITY_MAX];
    rogue_loot_stats_snapshot(counts);
    double factors[ROGUE_RARITY_MAX];
    for(int i=0;i<ROGUE_RARITY_MAX;i++) factors[i] = rogue_loot_dyn_get_factor(i);
    time_t t = time(NULL);
    fprintf(f, "{\n  \"timestamp\": %ld,\n  \"rarity_counts\": [", (long)t);
    for(int i=0;i<ROGUE_RARITY_MAX;i++){ if(i) fputc(',', f); fprintf(f, "%d", counts[i]); }
    fputs("],\n  \"dynamic_factors\": [", f);
    for(int i=0;i<ROGUE_RARITY_MAX;i++){ if(i) fputc(',', f); fprintf(f, "%.3f", factors[i]); }
    fprintf(f, "],\n  \"window_size\": %d\n}\n", ROGUE_LOOT_STATS_CAP);
    fclose(f);
    return 0;
}
