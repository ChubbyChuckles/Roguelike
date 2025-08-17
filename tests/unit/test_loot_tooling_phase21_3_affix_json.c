#include <stdio.h>
#include <string.h>
#include "core/loot_affixes.h"

static int write_cfg(const char* path){
    FILE* f=fopen(path, "wb"); if(!f) return -1;
    fprintf(f, "PREFIX,flat_dmg,damage_flat,1,5,10,0,0,0,0\n");
    fprintf(f, "SUFFIX,agi_boost,agility_flat,2,3,5,5,5,5,5\n");
    fclose(f); return 0; }

int main(void){
    const char* path="temp_affixes.cfg";
    if(write_cfg(path)!=0){ fprintf(stderr, "FAIL: cfg write\n"); return 1; }
    if(rogue_affixes_reset()!=0){ fprintf(stderr, "FAIL: reset\n"); return 2; }
    int added = rogue_affixes_load_from_cfg(path);
    if(added < 2){ fprintf(stderr, "FAIL: expected >=2 added got %d\n", added); return 3; }
    char buf[512];
    int len = rogue_affixes_export_json(buf, sizeof buf);
    if(len < 0){ fprintf(stderr, "FAIL: export\n"); return 4; }
    if(strstr(buf, "flat_dmg")==NULL || strstr(buf, "agi_boost")==NULL){ fprintf(stderr, "FAIL: missing ids in json %s\n", buf); return 5; }
    if(strstr(buf, "\"min\":1")==NULL || strstr(buf, "\"max\":5")==NULL){ fprintf(stderr, "FAIL: missing min/max\n"); return 6; }
    remove(path);
    printf("loot_tooling_phase21_3_affix_json_ok len=%d\n", len);
    return 0; }
