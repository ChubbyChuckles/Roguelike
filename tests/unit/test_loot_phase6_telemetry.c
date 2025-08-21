#include <stdio.h>
#include <string.h>
#include "core/loot/loot_console.h"
#include "core/loot/loot_stats.h"

/* Simple unit test for telemetry export (6.5) */
int main(){
    /* record some rarity occurrences */
    rogue_loot_stats_record_rarity(0);
    rogue_loot_stats_record_rarity(2);
    rogue_loot_stats_record_rarity(4);
    const char* path = "telemetry_test.json";
    int r = rogue_loot_export_telemetry(path);
    if(r!=0){ printf("TELEMETRY_FAIL export r=%d\n", r); return 1; }
    FILE* f = fopen(path, "rb");
    if(!f){ puts("TELEMETRY_FAIL no file"); return 1; }
    char buf[512]; size_t n=fread(buf,1,sizeof(buf)-1,f); buf[n]='\0'; fclose(f);
    if(!strstr(buf, "rarity_counts") || !strstr(buf, "dynamic_factors") || !strstr(buf, "window_size")){
        puts("TELEMETRY_FAIL missing keys");
        puts(buf);
        return 1; }
    puts("TELEMETRY_OK");
    return 0;
}
