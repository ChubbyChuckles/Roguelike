#include "core/save_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Define two lightweight dummy components to exercise incremental caching */
static int g_dummy_a_value = 1234; /* component id 1 */
static int g_dummy_b_value = 5678; /* component id 2 */
static int write_dummy_a(FILE* f){ fwrite(&g_dummy_a_value,sizeof g_dummy_a_value,1,f); return 0; }
static int read_dummy_a(FILE* f, size_t size){ if(size < sizeof(int)) return -1; fread(&g_dummy_a_value,sizeof g_dummy_a_value,1,f); return 0; }
static int write_dummy_b(FILE* f){ fwrite(&g_dummy_b_value,sizeof g_dummy_b_value,1,f); return 0; }
static int read_dummy_b(FILE* f, size_t size){ if(size < sizeof(int)) return -1; fread(&g_dummy_b_value,sizeof g_dummy_b_value,1,f); return 0; }

/* Buff stub (deduplicated) */
/* Use real buff system */

int main(void){
    setvbuf(stdout,NULL,_IONBF,0);
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    /* Register only our dummy components (ids 1 & 2) */
    RogueSaveComponent A={1,write_dummy_a,read_dummy_a,"A"};
    RogueSaveComponent B={2,write_dummy_b,read_dummy_b,"B"};
    rogue_save_manager_register(&A);
    rogue_save_manager_register(&B);
    rogue_save_set_incremental(1);
    if(rogue_save_manager_save_slot(0)!=0){ printf("INCR_FAIL initial save\n"); return 1; }
    FILE* f1=fopen("save_slot_0.sav","rb"); if(!f1){ printf("INCR_FAIL open1\n"); return 1; } fseek(f1,0,SEEK_END); long sz1=ftell(f1); fclose(f1);
    if(rogue_save_manager_save_slot(0)!=0){ printf("INCR_FAIL second save\n"); return 1; }
    FILE* f2=fopen("save_slot_0.sav","rb"); if(!f2){ printf("INCR_FAIL open2\n"); return 1; } fseek(f2,0,SEEK_END); long sz2=ftell(f2); fclose(f2);
    if(sz1!=sz2){ printf("INCR_FAIL size_mismatch %ld %ld\n", sz1, sz2); return 1; }
    /* Modify only component A and mark it dirty */
    g_dummy_a_value = 4321; rogue_save_mark_component_dirty(1);
    if(rogue_save_manager_save_slot(0)!=0){ printf("INCR_FAIL third save dirty\n"); return 1; }
    printf("INCR_OK sz=%ld a=%d b=%d\n", sz2, g_dummy_a_value, g_dummy_b_value);
    return 0; }
