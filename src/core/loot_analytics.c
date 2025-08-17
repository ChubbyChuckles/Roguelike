#include "core/loot_analytics.h"
#include <string.h>
#include <stdio.h>

static struct {
    RogueLootDropEvent ring[ROGUE_LOOT_ANALYTICS_RING_CAP];
    int head; /* next write */
    int count; /* <= cap */
    int rarity_counts[5];
} g_la;

void rogue_loot_analytics_reset(void){ memset(&g_la,0,sizeof g_la); }

void rogue_loot_analytics_record(int def_index, int rarity, double t_seconds){
    if(rarity<0||rarity>=5) rarity=0;
    RogueLootDropEvent* e = &g_la.ring[g_la.head];
    *e = (RogueLootDropEvent){ def_index, rarity, t_seconds };
    g_la.head = (g_la.head + 1) % ROGUE_LOOT_ANALYTICS_RING_CAP;
    if(g_la.count < ROGUE_LOOT_ANALYTICS_RING_CAP) g_la.count++;
    g_la.rarity_counts[rarity]++;
}

int rogue_loot_analytics_count(void){ return g_la.count; }

int rogue_loot_analytics_recent(int max, RogueLootDropEvent* out){
    if(max<=0||!out) return 0;
    int n = g_la.count < max ? g_la.count : max;
    for(int i=0;i<n;i++){
        int idx = (g_la.head - 1 - i);
        if(idx<0) idx += ROGUE_LOOT_ANALYTICS_RING_CAP;
        out[i] = g_la.ring[idx];
    }
    return n;
}

void rogue_loot_analytics_rarity_counts(int out_counts[5]){ if(!out_counts) return; for(int i=0;i<5;i++) out_counts[i]=g_la.rarity_counts[i]; }

int rogue_loot_analytics_export_json(char* buf, int cap){
    if(!buf||cap<=0) return -1;
    int rc[5]; rogue_loot_analytics_rarity_counts(rc);
    int written = snprintf(buf, (size_t)cap, "{\"drop_events\":%d,\"rarity_counts\":[%d,%d,%d,%d,%d]}", g_la.count, rc[0],rc[1],rc[2],rc[3],rc[4]);
    if(written<0 || written>=cap) return -2;
    return 0;
}
