/* Phase 7 determinism test: seed -> series of RNG draws + journal entries must reproduce identically */
#include "core/rng_streams.h"
#include "core/crafting_journal.h"
#include <stdio.h>
#include <string.h>

static unsigned int simulate_session(unsigned int seed){
	rogue_rng_streams_seed(seed);
	rogue_craft_journal_reset();
	/* simulate 50 mixed draws across streams and append journal */
	unsigned int accum=0;
	for(int i=0;i<50;i++){
		int stream = i%ROGUE_RNG_STREAM_COUNT; /* cycle streams */
		unsigned int r = rogue_rng_next((RogueRngStream)stream);
		accum ^= (r << (stream*3));
		unsigned int pre = accum & 0xFFFF;
		unsigned int post = (accum ^ r) & 0xFFFF;
		rogue_craft_journal_append(100+stream, pre, post, (unsigned int)stream, r ^ (pre<<1) ^ 0x9E3779B9u);
	}
	/* fold final accum hash with journal hash */
	return rogue_craft_journal_accum_hash() ^ (accum*0xA24BAEDCu);
}

int main(void){
	unsigned int s1 = simulate_session(77777u);
	unsigned int s2 = simulate_session(77777u);
	if(s1!=s2){
		fprintf(stderr,"P7_FAIL replay_hash_mismatch %u vs %u\n", s1,s2);
		return 20;
	}
	/* different seed should (overwhelmingly) differ */
	unsigned int s3 = simulate_session(88888u);
	if(s1==s3){
		fprintf(stderr,"P7_FAIL seed_collision %u vs %u\n", s1,s3);
		return 21;
	}
	printf("CRAFT_P7_DET_OK hash=%u alt=%u entries=%d\n", s1,s3, rogue_craft_journal_count());
	return 0;
}
