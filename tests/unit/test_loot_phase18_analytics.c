#include "core/loot/loot_analytics.h"
#include <stdio.h>
#include <string.h>

static int expect(int cond, const char* msg){ if(!cond){ printf("FAIL: %s\n", msg); return 0;} return 1; }

int main(void){
    rogue_loot_analytics_reset();
    for(int i=0;i<600;i++){
        rogue_loot_analytics_record(100+i, i%5, (double)i*0.1);
    }
    int count = rogue_loot_analytics_count();
    int pass=1; pass &= expect(count==ROGUE_LOOT_ANALYTICS_RING_CAP, "ring cap count");
    RogueLootDropEvent evs[4];
    int got = rogue_loot_analytics_recent(4, evs);
    pass &= expect(got==4, "recent count");
    pass &= expect(evs[0].def_index==100+599, "latest def index");
    pass &= expect(evs[3].def_index==100+596, "4th latest def index");
    int rc[5]; rogue_loot_analytics_rarity_counts(rc);
    for(int r=0;r<5;r++){ char buf[64]; snprintf(buf,sizeof buf,"rarity count r=%d",r); pass &= expect(rc[r]==120, buf); }
    char json[256]; int jrc = rogue_loot_analytics_export_json(json,sizeof json); pass &= expect(jrc==0, "json export ok");
    pass &= expect(strstr(json,"\"drop_events\":512")!=NULL, "json drop_events");
    pass &= expect(strstr(json,"\"rarity_counts\":[120,120,120,120,120]")!=NULL, "json rarity counts");
    if(pass) printf("OK\n");
    return pass?0:1;
}
