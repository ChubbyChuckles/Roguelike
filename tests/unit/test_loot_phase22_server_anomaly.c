#include <stdio.h>
#include "core/loot_security.h"

static int expect(int cond, const char* msg){ if(!cond){ fprintf(stderr,"FAIL %s\n", msg); return 0;} return 1; }

int main(void){
    /* Server verify deterministic */
    int ids[3]={2,4,6}; int qty[3]={1,1,1}; int rar[3]={0,1,4}; unsigned int seed=98765u;
    uint32_t h = rogue_loot_roll_hash(3,seed,3,ids,qty,rar);
    int vr = rogue_loot_server_verify(3,seed,3,ids,qty,rar,h);
    if(!expect(vr==0, "server verify match")) return 1;
    int vrb = rogue_loot_server_verify(3,seed,3,ids,qty,rar,h^0x1); if(!expect(vrb==1, "server verify mismatch")) return 2;

    /* Anomaly detector: configure low baseline to trigger spike */
    rogue_loot_anomaly_reset();
    rogue_loot_anomaly_config(256, 0.05f, 2.0f, 2);
    int high_roll_rar[5]={4,4,4,3,4};
    for(int i=0;i<5;i++){ rogue_loot_anomaly_record(5, high_roll_rar); }
    if(!expect(rogue_loot_anomaly_flag()==1, "anomaly flagged")) return 3;

    rogue_loot_anomaly_reset();
    int normal_rar[5]={0,1,0,2,1};
    for(int i=0;i<20;i++){ rogue_loot_anomaly_record(5, normal_rar); }
    if(!expect(rogue_loot_anomaly_flag()==0, "no anomaly")) return 4;

    printf("loot_phase22_server_anomaly_ok\n");
    return 0;
}
