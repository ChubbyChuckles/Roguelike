/* Phase 18.1: Golden master snapshot export & compare test */
#include <stdio.h>
#include <string.h>
#include "core/equipment/equipment_persist.h"
#include "core/stat_cache.h"
#include "core/equipment/equipment.h"

static void fail(const char* m){ printf("FAIL:%s\n", m); fflush(stdout); exit(1);} 
static void ck(int c,const char* m){ if(!c) fail(m); }

/* Minimal helper spawning a dummy item definition & instance if available; fallback no-op if API absent. */
extern int rogue_items_spawn(int def_index,int quantity,float x,float y);
extern const void* rogue_item_def_at(int index);

int main(void){
    char snap[128];
    int n = rogue_equipment_snapshot_export(snap, sizeof snap);
    ck(n>0 && strstr(snap,"EQSNAP v1")!=NULL, "snapshot export baseline");
    ck(rogue_equipment_snapshot_compare(snap)==0, "compare match baseline");
    /* Attempt to mutate equipment state (spawn + equip) if spawn API present to force mismatch.
       Also force stat cache update (fingerprint) if equip succeeded. */
    int mutation_attempted = 0;
    if(rogue_item_def_at(0)!=NULL){
        int inst = rogue_items_spawn(0,1,0,0);
        if(inst>=0){
            if(rogue_equip_get(ROGUE_EQUIP_WEAPON)<0){
                if(rogue_equip_try(ROGUE_EQUIP_WEAPON, inst)==0){ mutation_attempted = 1; }
            }
        }
    }
    if(mutation_attempted){
        extern void rogue_stat_cache_force_update(const void* p); /* player pointer unused placeholder */
        rogue_stat_cache_force_update(NULL);
    }
    char snap2[128]; int n2 = rogue_equipment_snapshot_export(snap2,sizeof snap2); ck(n2>0, "snapshot export post-mutate");
    int cmp = rogue_equipment_snapshot_compare(snap); /* compare old snapshot vs new state */
    if(mutation_attempted){
        ck(cmp==1, "compare mismatch after change");
    } else {
        ck(cmp==0, "no mutation path stable snapshot");
    }
    printf("Phase18.1 snapshot OK (%s -> %s)\n", snap, snap2);
    return 0;
}
