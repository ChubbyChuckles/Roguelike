/* Test v8 tamper flags and recovery */
#include "core/save_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Buff stubs */
typedef struct RogueBuff { int type; int active; float magnitude; double end_ms; } RogueBuff; RogueBuff g_buffs_internal[2]; int g_buff_count_internal=0; void rogue_buffs_apply(int t,float m,double end,float now){ (void)t;(void)m;(void)end;(void)now; }

static void corrupt_file(const char* path){ FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"r+b");
#else
    f=fopen(path,"r+b");
#endif
    if(!f) return; /* Seek near end to flip a CRC byte (last 40 bytes hold footer & maybe CRC) */
    fseek(f, -20, SEEK_END); long pos=ftell(f); if(pos>0){ unsigned char b=0; fread(&b,1,1,f); b ^= 0xA5; fseek(f,-1,SEEK_CUR); fwrite(&b,1,1,f); }
    fclose(f); }

int main(void){
    setvbuf(stdout,NULL,_IONBF,0);
    if(ROGUE_SAVE_FORMAT_VERSION < 8u){ printf("TAMPER_SKIP v=%u\n", (unsigned)ROGUE_SAVE_FORMAT_VERSION); return 0; }
        printf("TAMPER_DBG start v=%u\n", (unsigned)ROGUE_SAVE_FORMAT_VERSION); fflush(stdout);
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    /* produce a valid save + autosave */
    if(rogue_save_manager_save_slot(0)!=0){ printf("TAMPER_FAIL save primary\n"); return 1; }
    if(rogue_save_manager_autosave(0)!=0){ printf("TAMPER_FAIL autosave\n"); return 1; }
    /* corrupt primary */
    corrupt_file("save_slot_0.sav");
    int rc = rogue_save_manager_load_slot_with_recovery(0);
    if(rc<0){ printf("TAMPER_FAIL recovery rc=%d flags=0x%X\n", rc, rogue_save_last_tamper_flags()); return 1; }
    printf("TAMPER_DBG after load rc=%d flags=0x%X recovery=%d\n", rc, rogue_save_last_tamper_flags(), rogue_save_last_recovery_used()); fflush(stdout);
    if(rc==0){ printf("TAMPER_FAIL expected recovery path flags=0x%X\n", rogue_save_last_tamper_flags()); return 1; }
    if(!rogue_save_last_recovery_used()){ printf("TAMPER_FAIL flag not set recovery_used\n"); return 1; }
    unsigned tf = rogue_save_last_tamper_flags(); /* descriptor CRC likely */
    if((tf & (ROGUE_TAMPER_FLAG_DESCRIPTOR_CRC | ROGUE_TAMPER_FLAG_SECTION_CRC | ROGUE_TAMPER_FLAG_SHA256))==0){ printf("TAMPER_FAIL no tamper flag tf=0x%X\n", tf); return 1; }
    printf("TAMPER_OK recovery rc=%d tamper_flags=0x%X\n", rc, tf);
    printf("TAMPER_DONE\n");
    return 0; }
