#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "snapshot_manager.h"
#include "state_validation_manager.h"

// Fake system state for validation
typedef struct { int health; int max_health; int version; } VState;
static VState g_vs;

static int cap_cb(void* user, void** out_data, size_t* out_size, uint32_t* out_version){ *out_size=sizeof(VState); *out_data=malloc(*out_size); if(!*out_data) return -1; memcpy(*out_data,user,*out_size); *out_version=(uint32_t)((VState*)user)->version; return 0; }
static int rest_cb(void* user,const void* data,size_t size,uint32_t ver){ if(size!=sizeof(VState)) return -2; memcpy(user,data,size); ((VState*)user)->version=(int)ver; return 0; }

static RogueValidationResult validate_sys(void* user){ VState* s=(VState*)user; if(s->health<0 || s->health>s->max_health){ RogueValidationResult r={ROGUE_VALID_CORRUPT,1,"health out of bounds"}; return r; } if(s->health > (s->max_health/2)){ RogueValidationResult r={ROGUE_VALID_WARN,2,"health above half"}; return r; } RogueValidationResult ok={ROGUE_VALID_OK,0,"ok"}; return ok; }
static int repair_sys(void* user, uint32_t code){ VState* s=(VState*)user; if(code==1){ if(s->health<0) s->health=0; if(s->health>s->max_health) s->health=s->max_health; return 0; } return -1; }

static RogueValidationResult cross_rule(void* user){ (void)user; RogueValidationResult ok={ROGUE_VALID_OK,0,"cross"}; return ok; }

int main(){
    memset(&g_vs,0,sizeof(g_vs)); g_vs.max_health=100; g_vs.health=40; g_vs.version=1;
    RogueSnapshotDesc d={0}; d.system_id=11; d.name="val_sys"; d.capture=cap_cb; d.restore=rest_cb; d.user=&g_vs; assert(rogue_snapshot_register(&d)==0);
    // initial capture to seed hash
    assert(rogue_snapshot_capture(11)==0);
    assert(rogue_validation_register_system(11,validate_sys,repair_sys,&g_vs)==0);
    assert(rogue_validation_register_cross_rule("noop",cross_rule,NULL)==0);
    rogue_validation_set_interval(5);
    // tick forward without changes (should eventually run)
    for(int t=1;t<=10;t++){ rogue_validation_tick((uint64_t)t); if(t==5 || t==10) assert(g_vs.health==40); }
    RogueValidationStats stats; rogue_validation_get_stats(&stats); assert(stats.runs_completed>=1);
    // induce warning boundary
    g_vs.health=60; g_vs.version=2; assert(rogue_snapshot_capture(11)==0); rogue_validation_run_now(0); rogue_validation_get_stats(&stats); assert(stats.warnings>=1);
    // induce corruption and ensure repair
    g_vs.health=1000; g_vs.version=3; assert(rogue_snapshot_capture(11)==0); rogue_validation_run_now(0); assert(g_vs.health==100); rogue_validation_get_stats(&stats); assert(stats.corruptions_detected>=1 && stats.repairs_succeeded>=1);
    // incremental skip test: capture unchanged version
    g_vs.version=4; // modify version triggers capture hash change
    assert(rogue_snapshot_capture(11)==0);
    rogue_validation_run_now(0);
    rogue_validation_get_stats(&stats);
    fprintf(stderr,"test_state_validation_manager OK\n");
    return 0;
}
