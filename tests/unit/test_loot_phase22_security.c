#include <stdio.h>
#include <string.h>
#include "core/loot/loot_security.h"

static int expect(int cond, const char* msg){ if(!cond){ fprintf(stderr,"FAIL %s\n", msg); return 0;} return 1; }

int main(void){
    /* Test 22.1: hash deterministic & sensitive to params */
    int ids[3]={1,5,9}; int qty[3]={2,1,4}; int rar[3]={0,2,4};
    unsigned int seed=12345u; uint32_t h1=rogue_loot_roll_hash(7,seed,3,ids,qty,rar);
    uint32_t h2=rogue_loot_roll_hash(7,seed,3,ids,qty,rar);
    if(!expect(h1==h2,"hash deterministic")) return 1;
    qty[1]++; uint32_t h3=rogue_loot_roll_hash(7,seed,3,ids,qty,rar); qty[1]--; if(!expect(h3!=h1,"hash detects qty change")) return 2;

    /* Test 22.2: seed obfuscation toggling */
    rogue_loot_security_enable_obfuscation(0); if(!expect(!rogue_loot_security_obfuscation_enabled(),"obfuscation off")) return 3;
    unsigned int salt=0xA5B6C7D8u; unsigned int ob1=rogue_loot_obfuscate_seed(seed,salt); unsigned int ob2=rogue_loot_obfuscate_seed(seed,salt); if(!expect(ob1==ob2,"obfuscation deterministic")) return 4;
    rogue_loot_security_enable_obfuscation(1); if(!expect(rogue_loot_security_obfuscation_enabled(),"obfuscation on")) return 5;

    /* Test 22.3: file snapshot + verify */
    const char* single[1]={"../README.md"};
    int sr = rogue_loot_security_snapshot_files(single,1);
    if(sr!=0){ fprintf(stderr,"snapshot rc=%d path=%s\n", sr, single[0]); if(!expect(0,"snapshot ok")) return 6; }
    int vr = rogue_loot_security_verify_files(single,1); if(!expect(vr==0,"verify unchanged")) return 7;

    /* Mismatch simulation: compute second hash using altered data by calling internal roll hash with different params and ensuring it differs (logic demonstration). */
    uint32_t alt = rogue_loot_roll_hash(7,seed,2,ids,qty,rar); /* drop_count changed */
    if(!expect(alt!=h1,"alt hash differs")) return 8;

    printf("loot_phase22_security_ok\n");
    return 0;
}
