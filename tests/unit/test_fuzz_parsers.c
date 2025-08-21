/* test_fuzz_parsers.c - Phase M4.4 fuzz tests for parser robustness
 * Targets:
 *  - loot_affixes (CSV style)
 *  - persistence player stats kv pairs
 *  - unified kv_parser (key/value)
 * Strategy: generate many malformed / randomized lines and feed into parsers ensuring:
 *  - No crash / UB (process completes)
 *  - Parser returns either success counts or ignores malformed lines gracefully
 *  - For persistence load, player version never becomes <=0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/loot/loot_affixes.h"
#include "core/persistence.h"
#include "core/app_state.h"
#include "util/kv_parser.h"

/* Forward declarations (implemented in existing translation units) */
extern void rogue_persistence_load_player_stats(void);
extern void rogue_persistence_save_player_stats(void);
extern const char* rogue__player_stats_path(void);
extern int rogue_persistence_player_version(void);

static unsigned int rng=0x12345678u;
static unsigned int r32(void){ rng = rng*1664525u + 1013904223u; return rng; }
static int r_range(int lo,int hi){ if(hi<=lo) return lo; return lo + (int)(r32()% (unsigned)(hi-lo+1)); }
static char r_char(void){ const char* set = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789#=,; \r\n"; return set[r32()%60]; }

static void make_temp_file(const char* path, const char* data){ FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"wb");
#else
    f=fopen(path,"wb");
#endif
    if(!f) return; fwrite(data,1,strlen(data),f); fclose(f); }

static void fuzz_affixes(void){
    rogue_affixes_reset();
    char buf[8192]; size_t pos=0; int lines=500; for(int i=0;i<lines && pos+100<sizeof buf; ++i){
        int form = r_range(0,3);
        if(form==0){
            const char* type = (r32()&1)? "PREFIX":"SUFFIX"; const char* stat=(r32()&1)?"damage_flat":"agility_flat";
            int min = r_range(-5,50); int max = min + r_range(0,50);
            pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "%s,ID%d,%s,%d,%d,%d,%d,%d,%d,%d\n", type,i,stat,min,max,r_range(0,10),r_range(0,10),r_range(0,10),r_range(0,10),r_range(0,10));
        } else if(form==1){
            pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "PREFIX,BAD%d,damage_flat,%d\n", i, r_range(0,10));
        } else if(form==2){
            int len=r_range(5,40); for(int k=0;k<len && pos+1<sizeof buf; ++k){ buf[pos++]=r_char(); } if(pos+1<sizeof buf) buf[pos++]='\n';
        } else {
            pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "# comment %d\n", i);
        }
    }
    buf[(pos<sizeof buf)? pos: sizeof buf -1] ='\0';
    make_temp_file("fuzz_affixes.cfg", buf);
    int added = rogue_affixes_load_from_cfg("fuzz_affixes.cfg");
    if(added < 0){ fprintf(stderr,"affix fuzz: loader returned error\n"); exit(1); }
    if(rogue_affix_count() >= ROGUE_MAX_AFFIXES){ fprintf(stderr,"affix fuzz: exceeded cap\n"); exit(1); }
}

static void fuzz_player_persistence(void){
    char buf[8192]; size_t pos=0; int lines=400; for(int i=0;i<lines && pos+50<sizeof buf; ++i){
        int form=r_range(0,4);
        if(form==0){ pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "LEVEL=%d\n", r_range(-10,200)); }
        else if(form==1){ pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "VERSION=%d\n", r_range(-5,5)); }
        else if(form==2){ pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "GI%d=%d,%d,%d,%d,%d,%d,%d\n", r_range(0,5), r_range(-2,10), r_range(-1,5), r_range(-1,4), r_range(-10,20), r_range(-1,4), r_range(-10,20), r_range(-5,30)); }
        else if(form==3){ pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "RNDKEY%d=%d\n", i, r_range(-1000,1000)); }
        else {
            int len=r_range(3,30); for(int k=0;k<len && pos+1<sizeof buf; ++k){ buf[pos++]=r_char(); } if(pos+1<sizeof buf) buf[pos++]='\n';
        }
    }
    buf[(pos<sizeof buf)? pos: sizeof buf -1]='\0';
    make_temp_file(rogue__player_stats_path(), buf);
    rogue_persistence_load_player_stats();
    if(rogue_persistence_player_version() <=0){ fprintf(stderr,"persistence fuzz: version clamp failure\n"); exit(1); }
}

static void fuzz_kv_parser(void){
    char buf[4096]; size_t pos=0; for(int i=0;i<300 && pos+20<sizeof buf; ++i){
        int form=r_range(0,5);
        if(form==0){ pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "KEY%d=VAL%d\n", i, r_range(0,999)); }
        else if(form==1){ pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "=NOVALUE%d\n", i); }
        else if(form==2){ pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "KEYONLY%d\n", i); }
        else if(form==3){ pos += (size_t)snprintf(buf+pos,sizeof buf - pos, "# comment %d\n", i); }
        else if(form==4){ pos += (size_t)snprintf(buf+pos,sizeof buf - pos, " SPACED%d =  %d  # trailing\n", i, r_range(0,50)); }
        else { int len=r_range(3,15); for(int k=0;k<len && pos+1<sizeof buf; ++k){ buf[pos++]=r_char(); } if(pos+1<sizeof buf) buf[pos++]='\n'; }
    }
    buf[(pos<sizeof buf)? pos: sizeof buf -1]='\0';
    make_temp_file("fuzz_kv.cfg", buf);
    RogueKVFile kv; if(!rogue_kv_load_file("fuzz_kv.cfg", &kv)){ fprintf(stderr,"kv fuzz: load failed\n"); exit(1);} 
    int cursor=0; RogueKVEntry e; int parsed=0; RogueKVError err; while(rogue_kv_next(&kv,&cursor,&e,&err)){ parsed++; if(parsed>500){ fprintf(stderr,"kv fuzz: runaway loop\n"); exit(1);} }
    rogue_kv_free(&kv);
}

int main(void){
    rng ^= (unsigned)time(NULL);
    fuzz_affixes();
    fuzz_player_persistence();
    fuzz_kv_parser();
    printf("fuzz_parsers_ok\n");
    return 0;
}
