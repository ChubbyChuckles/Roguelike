/* Phase 18.5: Mutation / corruption robustness test.
   Goals:
     1. Deserialize baseline valid snapshot – hash matches original.
     2. Targeted invalid slot index mutation -> parser returns negative (rejection) not crash.
     3. Random single-bit flips across serialized buffer (N iterations) never crash; each either:
          - returns 0 (best-effort load; state hash produced), OR
          - returns negative error code signalling rejection.
        We require at least one rejection to prove negative path exercised.
     4. Corrupt durability numeric (flip digit) produces different state hash (tamper detection still meaningful).
   The test avoids undefined behaviour by always operating within buffer bounds and resetting equip state between attempts.
*/
#define SDL_MAIN_HANDLED 1
#include "core/loot_instances.h" /* for g_rogue_loot_suppress_spawn_log */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/equipment_persist.h"
#include "core/equipment.h"
#include "core/loot_instances.h"

extern int rogue_items_spawn(int def_index,int quantity,float x,float y);
extern unsigned long long rogue_equipment_state_hash(void);

static void reset_env(void){
    /* Also reset item instance pool to avoid cumulative spawn pool saturation
       during fuzz iterations (prevents noisy 'pool full' warnings). */
    rogue_items_init_runtime();
    rogue_equip_reset();
}

static unsigned int lcg_next(unsigned int* s){ *s = (*s * 1664525u + 1013904223u); return *s; }

int main(void){
    g_rogue_loot_suppress_spawn_log = 1; /* runtime silence of spawn INFO lines */
    /* Spawn & equip baseline items (defs 0..1 assumed valid in test asset set) */
    int a = rogue_items_spawn(0,1,0,0); assert(a>=0); assert(rogue_equip_try(ROGUE_EQUIP_WEAPON,a)==0);
    int b = rogue_items_spawn(1,1,0,0); assert(b>=0); assert(rogue_equip_try(ROGUE_EQUIP_RING1,b)==0);

    char buf[4096];
    int n = rogue_equipment_serialize(buf,(int)sizeof buf); assert(n>0);
    unsigned long long base_hash = rogue_equipment_state_hash(); assert(base_hash!=0ULL);

    /* 1. Baseline round-trip */
    reset_env();
    assert(rogue_equipment_deserialize(buf)==0);
    unsigned long long h2 = rogue_equipment_state_hash();
    assert(h2==base_hash);

    /* 2. Targeted invalid slot mutation: replace first occurrence of "SLOT " number with 999 */
    {
        char mut[4096]; strncpy(mut,buf,sizeof mut); mut[sizeof mut-1]='\0';
        char* p = strstr(mut,"SLOT ");
        if(p){ p += 5; p[0]='9'; p[1]='9'; p[2]='9'; }
        reset_env();
        int r = rogue_equipment_deserialize(mut);
        assert(r<0); /* expect rejection */
    }

    /* 3. Random single-bit flips */
    {
        unsigned int seed = 123456789u;
        int iterations = 200;
        int rejects = 0, successes = 0;
        for(int i=0;i<iterations;i++){
            char mut[4096]; memcpy(mut,buf,(size_t)n+1);
            unsigned int pos = lcg_next(&seed) % (unsigned int)((n>0)?n:1);
            unsigned int bit = lcg_next(&seed) & 7u;
            mut[pos] ^= (char)(1u<<bit);
            reset_env();
            int r = rogue_equipment_deserialize(mut);
            if(r<0) rejects++; else successes++;
        }
        assert(rejects>0);
        assert(successes>0);
    }

    /* 4. Durability digit tamper -> hash changes */
    {
        char tam[4096]; strncpy(tam,buf,sizeof tam); tam[sizeof tam-1]='\0';
        char* d = strstr(tam," DUR ");
        if(d){ while(*d && *d!='\n' && (*d<'0'||*d>'9')) d++; if(*d>='0'&&*d<='9') *d = (*d=='9')?'1':'9'; }
        reset_env();
        assert(rogue_equipment_deserialize(tam)==0);
        unsigned long long h3 = rogue_equipment_state_hash();
        assert(h3!=base_hash);
    }

    printf("Phase18.5 mutation robustness OK\n");
    return 0;
}
