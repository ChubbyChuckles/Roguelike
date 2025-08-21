#include "core/crafting/rng_streams.h"
#include "core/crafting_journal.h"
#include <stdio.h>
#include <assert.h>

int main(void){
    rogue_rng_streams_seed(12345u);
    unsigned int a = rogue_rng_next(ROGUE_RNG_STREAM_GATHERING);
    unsigned int b = rogue_rng_next(ROGUE_RNG_STREAM_REFINEMENT);
    unsigned int c = rogue_rng_next(ROGUE_RNG_STREAM_GATHERING);
    /* streams must be independent: a != b and advancing gathering again not equal to previous refinement sample typically */
    if(a==b){ fprintf(stderr,"P7_FAIL stream_independence\n"); return 10; }
    rogue_craft_journal_reset();
    unsigned int h_before = rogue_craft_journal_accum_hash();
    int id0 = rogue_craft_journal_append(1,10,12,ROGUE_RNG_STREAM_GATHERING, a^0xABCD1234u);
    int id1 = rogue_craft_journal_append(1,12,15,ROGUE_RNG_STREAM_REFINEMENT, b^0xABCD1234u);
    if(id0!=0||id1!=1){ fprintf(stderr,"P7_FAIL journal_ids\n"); return 11; }
    unsigned int h_after = rogue_craft_journal_accum_hash();
    if(h_before==h_after){ fprintf(stderr,"P7_FAIL hash_progress\n"); return 12; }
    printf("CRAFT_P7_OK streams a=%u b=%u c=%u count=%d hash=%u\n", a,b,c, rogue_craft_journal_count(), h_after);
    return 0;
}
