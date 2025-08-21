#include "core/loot/loot_commands.h"
#include "core/loot/loot_dynamic_weights.h"
#include "core/loot/loot_stats.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

static void wr(char* out,int out_sz,const char* fmt,...){
    if(!out||out_sz<=0) return; va_list ap; va_start(ap,fmt); vsnprintf(out,out_sz,fmt,ap); va_end(ap);
}

int rogue_loot_run_command(const char* line, char* out, int out_sz){
    if(!line){ wr(out,out_sz,"ERR: null line"); return 1; }
    while(isspace((unsigned char)*line)) line++;
    if(!*line){ wr(out,out_sz,"ERR: empty"); return 1; }
    char cmd[32]; int consumed=0;
#if defined(_MSC_VER)
    if(sscanf_s(line,"%31s%n",cmd,(unsigned)sizeof(cmd),&consumed)!=1){ wr(out,out_sz,"ERR: token"); return 1; }
#else
    if(sscanf(line,"%31s%n",cmd,&consumed)!=1){ wr(out,out_sz,"ERR: token"); return 1; }
#endif
    for(char* p=cmd; *p; ++p) *p=(char)tolower((unsigned char)*p);
    const char* args = line+consumed; while(isspace((unsigned char)*args)) args++;
    if(strcmp(cmd,"weight")==0){
    int rarity; float factor;
#if defined(_MSC_VER)
    if(sscanf_s(args,"%d %f", &rarity, &factor)!=2){ wr(out,out_sz,"ERR: usage weight <rarity 0-4> <factor>"); return 2; }
#else
    if(sscanf(args,"%d %f", &rarity, &factor)!=2){ wr(out,out_sz,"ERR: usage weight <rarity 0-4> <factor>"); return 2; }
#endif
        if(rarity<0||rarity>4){ wr(out,out_sz,"ERR: rarity range"); return 2; }
        rogue_loot_dyn_set_factor(rarity,factor); wr(out,out_sz,"OK: weight r%d=%.3f", rarity, rogue_loot_dyn_get_factor(rarity)); return 0;
    } else if(strcmp(cmd,"reset_dyn")==0){
        rogue_loot_dyn_reset(); wr(out,out_sz,"OK: dyn reset"); return 0;
    } else if(strcmp(cmd,"reset_stats")==0){
        rogue_loot_stats_reset(); wr(out,out_sz,"OK: stats reset"); return 0;
    } else if(strcmp(cmd,"stats")==0){
        int counts[5]; rogue_loot_stats_snapshot(counts); wr(out,out_sz,"STATS: C=%d U=%d R=%d E=%d L=%d", counts[0],counts[1],counts[2],counts[3],counts[4]); return 0;
    } else if(strcmp(cmd,"get")==0){
    int rarity; if(
#if defined(_MSC_VER)
        sscanf_s(args,"%d", &rarity)
#else
        sscanf(args,"%d", &rarity)
#endif
        !=1 || rarity<0||rarity>4){ wr(out,out_sz,"ERR: usage get <rarity 0-4>"); return 2; }
        wr(out,out_sz,"FACTOR: r%d=%.3f", rarity, rogue_loot_dyn_get_factor(rarity)); return 0;
    }
    wr(out,out_sz,"ERR: unknown cmd"); return 2;
}
