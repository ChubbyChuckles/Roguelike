/* Phase 17.4 cache-friendly item def index tests */
#include "core/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

static int fail=0; 
#define CHECK(c,msg) do{ if(!(c)){ printf("FAIL:%s line %d %s\n",__FILE__,__LINE__,msg); fail=1; } }while(0)

/* Minimal synthetic injection by directly populating definitions via loader assumptions: we simulate by constructing temporary CSV in memory written to a file. */
static void write_cfg(const char* path){ FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"wb");
#else
    f=fopen(path,"wb");
#endif
    if(!f) return; 
    /* id,name,cat,level,stack,value,dmg_min,dmg_max,armor,sheet,tx,ty,tw,th,rarity */
    fprintf(f,"itm_a,Item A,2,1,1,10,2,4,0,sheet,0,0,16,16,1\n");
    fprintf(f,"itm_b,Item B,3,1,1,12,1,3,0,sheet,0,0,16,16,2\n");
    fprintf(f,"itm_c,Item C,4,1,1,14,5,9,0,sheet,0,0,16,16,3\n");
    fclose(f);
}

int main(void){ setvbuf(stdout,NULL,_IONBF,0); rogue_item_defs_reset();
    write_cfg("test_items_tmp.cfg"); int added = rogue_item_defs_load_from_cfg("test_items_tmp.cfg"); CHECK(added==3,"added_three");
    int rc = rogue_item_defs_build_index(); CHECK(rc==0,"build_index_ok");
    int i_a = rogue_item_def_index_fast("itm_a"); int i_b = rogue_item_def_index_fast("itm_b"); int i_c = rogue_item_def_index_fast("itm_c");
    CHECK(i_a>=0 && i_b>=0 && i_c>=0,"fast_indices_valid");
    CHECK(i_a != i_b && i_b != i_c && i_a != i_c, "distinct_indices");
    /* Validate fallback equals fast */
    CHECK(i_a==rogue_item_def_index("itm_a"),"linear_match_a");
    CHECK(rogue_item_def_index_fast("missing")==-1,"missing_neg1");
    if(fail){ printf("FAILURES\n"); return 1; }
    printf("OK:loot_phase17_index\n"); return 0; }
