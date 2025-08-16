/* Validates that a legacy v1 save (header downgrade) migrates to v2 via registered chain */
#include "core/save_manager.h"
#include "core/app_state.h"
#include <stdio.h>
#include <string.h>

/* Buff stubs (same as other persistence tests) */
typedef struct RogueBuff { int type; int active; float magnitude; double end_ms; } RogueBuff;
RogueBuff g_buffs_internal[4];
int g_buff_count_internal=0;
void rogue_buffs_apply(int t,float m,double end,float now){ (void)t;(void)m;(void)end;(void)now; }

static int migrate_v1_to_v2(unsigned char* data, size_t size){ (void)data; (void)size; return 0; }
static RogueSaveMigration MIG1 = {1u,2u,migrate_v1_to_v2,"v1_to_v2"};

int main(void){
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    if(rogue_save_manager_save_slot(0)!=0){ printf("MIGRATION_FAIL initial_save\n"); return 1; }
    /* Downgrade header version to 1 */
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "save_slot_0.sav", "r+b");
#else
    f=fopen("save_slot_0.sav","r+b");
#endif
    if(!f){ printf("MIGRATION_FAIL open\n"); return 1; }
    unsigned version=1u; if(fwrite(&version,sizeof version,1,f)!=1){ fclose(f); printf("MIGRATION_FAIL write_version\n"); return 1; }
    fclose(f);
    /* Register migration and attempt load */
    rogue_save_register_migration(&MIG1);
    int rc = rogue_save_manager_load_slot(0);
    if(rc!=0){ printf("MIGRATION_FAIL rc=%d\n", rc); return 1; }
    printf("MIGRATION_OK v1_to_v2 rc=%d\n", rc);
    return 0;
}
