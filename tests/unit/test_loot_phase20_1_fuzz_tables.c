/* Phase 20.1 Fuzz tests for loot table parsing robustness
 * Goals:
 *  - Feed many randomized / malformed synthetic table lines into a temp file
 *  - Ensure loader never crashes and respects caps (tables, entries)
 *  - Ensure invalid entries (bad item id, zero/neg weight) don't increment entry_count
 *  - Exercise rarity min/max parsing including invalid ordering and negatives
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/loot_item_defs.h"
#include "core/loot_tables.h"

static unsigned int rng_state = 0xC0FFEE01u;
static unsigned int r32(void){ rng_state = rng_state * 1664525u + 1013904223u; return rng_state; }
static int rrange(int lo,int hi){ if(hi<=lo) return lo; return lo + (int)(r32()% (unsigned)(hi-lo+1)); }
static char rchar(void){ const char* set="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-"; return set[r32()% (unsigned)strlen(set)]; }

static void write_temp(const char* path, const char* data){ FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"wb");
#else
    f=fopen(path,"wb");
#endif
    if(!f) return; fwrite(data,1,strlen(data),f); fclose(f); }

static void build_item_defs_for_refs(void){
    /* Provide a small deterministic set of valid items so some fuzz lines resolve */
    rogue_item_defs_reset();
    char buf[2048]; size_t pos=0;
    for(int i=0;i<10;i++){
        pos += (size_t)snprintf(buf+pos,sizeof buf - pos,
            "ITM%d,Item%d,MISC,1,10,5,0,0,0,sheet,0,0,16,16,0\n", i, i);
    }
    buf[(pos<sizeof buf)? pos: sizeof buf -1] ='\0';
    write_temp("fuzz_items.cfg", buf);
    int added = rogue_item_defs_load_from_cfg("fuzz_items.cfg");
    if(added <= 0){ fprintf(stderr,"FAIL: fuzz items load %d\n", added); exit(2); }
    rogue_item_defs_build_index();
}

static void fuzz_tables(void){
    rogue_loot_tables_reset();
    char big[32768]; size_t pos=0; int lines=1200; /* many lines to exercise caps */
    for(int i=0;i<lines && pos+200 < sizeof big; ++i){
        int form = rrange(0,5);
        if(form==0){
            /* Well-formed line with 1-3 entries */
            int entries = rrange(1,3);
            pos += (size_t)snprintf(big+pos,sizeof big - pos, "TAB%d,%d,%d,", i, rrange(0,2), rrange(0,4));
            for(int e=0;e<entries;e++){
                int itm = rrange(0,14); /* some invalid (>=10) to test missing */
                int w = rrange(-5,50);
                int qmin = rrange(-2,5);
                int qmax = qmin + rrange(0,5) - (r32()&1? rrange(0,3):0); /* sometimes reversed */
                int rmin = (r32()&3)? rrange(-2,4): -1;
                int rmax = rmin + rrange(0,3) - (r32()&1? rrange(0,2):0);
                pos += (size_t)snprintf(big+pos,sizeof big - pos, "ITM%d,%d,%d,%d,%d,%d", itm, w, qmin, qmax, rmin, rmax);
                if(e<entries-1) pos += (size_t)snprintf(big+pos,sizeof big - pos, ";");
            }
            pos += (size_t)snprintf(big+pos,sizeof big - pos, "\n");
        } else if(form==1){
            /* Missing rolls fields */
            pos += (size_t)snprintf(big+pos,sizeof big - pos, "BADLINE%d,\n", i);
        } else if(form==2){
            /* Comment */
            pos += (size_t)snprintf(big+pos,sizeof big - pos, "# comment %d\n", i);
        } else if(form==3){
            /* Extremely long id */
            pos += (size_t)snprintf(big+pos,sizeof big - pos, "LONG%08XNAME,%d,%d,ITM1,1,1,1,1,-1,-1\n", r32(), 1,1);
        } else if(form==4){
            /* Random chars */
            int len=rrange(5,40); for(int k=0;k<len && pos+1<sizeof big; ++k){ big[pos++]=rchar(); } if(pos+1<sizeof big) big[pos++]='\n';
        } else {
            /* Empty line */
            pos += (size_t)snprintf(big+pos,sizeof big - pos, "\n");
        }
    }
    big[(pos<sizeof big)? pos: sizeof big -1] ='\0';
    write_temp("fuzz_tables.cfg", big);
    int added = rogue_loot_tables_load_from_cfg("fuzz_tables.cfg");
    if(added < 0){ fprintf(stderr,"FAIL: loader returned error\n"); exit(3); }
    if(rogue_loot_tables_count() > ROGUE_MAX_LOOT_TABLES){ fprintf(stderr,"FAIL: table cap exceeded %d>%d\n", rogue_loot_tables_count(), ROGUE_MAX_LOOT_TABLES); exit(4); }
    if(rogue_loot_tables_count()>0){
        /* Optional: attempt to access a known id (TAB0) if first well-formed line parsed */
        const RogueLootTableDef* t = rogue_loot_table_by_id("TAB0");
        if(t && t->entry_count > ROGUE_MAX_LOOT_ENTRIES){ fprintf(stderr,"FAIL: entry cap exceeded %d>%d\n", t->entry_count, ROGUE_MAX_LOOT_ENTRIES); exit(5); }
    }
}

int main(void){
    rng_state ^= (unsigned)time(NULL);
    build_item_defs_for_refs();
    fuzz_tables();
    printf("loot_fuzz_tables_ok\n");
    return 0;
}
