#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "core/progression_stats.h"

static void test_uniqueness_and_order(void){
    size_t n=0; const RogueStatDef* all = rogue_stat_def_all(&n); assert(n>0);
    /* Ensure strictly ascending IDs */
    int last=-1; for(size_t i=0;i<n;i++){ assert(all[i].id > last); last = all[i].id; }
    /* Ensure uniqueness of codes */
    for(size_t i=0;i<n;i++){
        for(size_t j=i+1;j<n;j++){
            assert(strcmp(all[i].code, all[j].code)!=0);
        }
    }
}

static void test_id_ranges(void){
    size_t n=0; const RogueStatDef* all = rogue_stat_def_all(&n);
    for(size_t i=0;i<n;i++){
        const RogueStatDef* d=&all[i];
        if(d->id < 0 || d->id >= 500) assert(!"id out of allowed master range (0-499)");
        if(d->id <100) assert(d->category==ROGUE_STAT_PRIMARY);
        else if(d->id <200) assert(d->category==ROGUE_STAT_DERIVED);
        else if(d->id <300) assert(d->category==ROGUE_STAT_RATING);
        else if(d->id <400) assert(d->category==ROGUE_STAT_MODIFIER);
        else {/* reserved future expansion (400-499) */}
    }
}

static void test_lookup_and_fingerprint(void){
    size_t n=0; const RogueStatDef* all = rogue_stat_def_all(&n); unsigned long long fp1 = rogue_stat_registry_fingerprint();
    /* Lookup each by id and pointer equality */
    for(size_t i=0;i<n;i++){ const RogueStatDef* d=rogue_stat_def_by_id(all[i].id); assert(d==&all[i]); }
    unsigned long long fp2 = rogue_stat_registry_fingerprint(); assert(fp1==fp2); /* deterministic */
}

int main(void){
    test_uniqueness_and_order();
    test_id_ranges();
    test_lookup_and_fingerprint();
    printf("progression_phase0_taxonomy: OK\n");
    return 0;
}
