/* Validates migration metrics counters when upgrading legacy header */
#include "core/save_manager.h"
#include <stdio.h>

/* Buff stubs */
typedef struct RogueBuff { int type; int active; float magnitude; double end_ms; } RogueBuff;
RogueBuff g_buffs_internal[2];
int g_buff_count_internal=0;
void rogue_buffs_apply(int t,float m,double end,float now){ (void)t;(void)m;(void)end;(void)now; }

static int migrate_v1_to_v2(unsigned char* data, size_t size){ (void)data; (void)size; return 0; }
static RogueSaveMigration MIG={1u,2u,migrate_v1_to_v2,"v1_to_v2"};

int main(void){
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    if(rogue_save_manager_save_slot(0)!=0){ printf("MIG_METRIC_FAIL save\n"); return 1; }
    /* Downgrade header version */
    FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,"save_slot_0.sav","r+b");
#else
    f=fopen("save_slot_0.sav","r+b");
#endif
    if(!f){ printf("MIG_METRIC_FAIL open\n"); return 1; }
    unsigned v=1u; if(fwrite(&v,sizeof v,1,f)!=1){ fclose(f); printf("MIG_METRIC_FAIL write_version\n"); return 1; }
    fclose(f);
    rogue_save_register_migration(&MIG);
    int rc=rogue_save_manager_load_slot(0); if(rc!=0){ printf("MIG_METRIC_FAIL rc=%d\n", rc); return 1; }
    if(rogue_save_last_migration_failed()){ printf("MIG_METRIC_FAIL unexpected_failed_flag\n"); return 1; }
    if(rogue_save_last_migration_steps()!=1){ printf("MIG_METRIC_FAIL steps=%d\n", rogue_save_last_migration_steps()); return 1; }
    double ms=rogue_save_last_migration_ms(); if(ms<0.0){ printf("MIG_METRIC_FAIL ms=%f\n", ms); return 1; }
    printf("MIG_METRIC_OK steps=%d ms=%.3f failed=%d\n", rogue_save_last_migration_steps(), ms, rogue_save_last_migration_failed());
    return 0;
}
