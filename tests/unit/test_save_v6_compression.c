/* Test v6 per-section compression (RLE) */
#include "core/save_manager.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Buff stubs */
typedef struct RogueBuff { int type; int active; float magnitude; double end_ms; } RogueBuff; RogueBuff g_buffs_internal[2]; int g_buff_count_internal=0; void rogue_buffs_apply(int t,float m,double end,float now){ (void)t;(void)m;(void)end;(void)now; }

static size_t fsize(const char* p){ struct stat st; if(stat(p,&st)!=0) return 0; return (size_t)st.st_size; }

int main(void){
    if(ROGUE_SAVE_FORMAT_VERSION < 6u){ printf("COMP_SKIP v=%u\n", (unsigned)ROGUE_SAVE_FORMAT_VERSION); return 0; }
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    rogue_save_set_compression(1, 16);
    /* Intern many duplicate strings to create good RLE compression */
    for(int i=0;i<50;i++){ char buf[16]; snprintf(buf,sizeof buf,"stat_%c", 'A'); rogue_save_intern_string(buf); }
    if(rogue_save_manager_save_slot(0)!=0){ printf("COMP_FAIL save\n"); return 1; }
    size_t sz = fsize("save_slot_0.sav"); if(sz==0){ printf("COMP_FAIL size\n"); return 1; }
    /* Load to ensure decompression path succeeds */
    if(rogue_save_manager_load_slot(0)!=0){ printf("COMP_FAIL load\n"); return 1; }
    printf("COMP_OK size=%zu\n", sz);
    return 0; }
