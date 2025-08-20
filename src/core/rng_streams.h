#ifndef ROGUE_RNG_STREAMS_H
#define ROGUE_RNG_STREAMS_H
/* Phase 7: Determinism & RNG Governance */
typedef enum RogueRngStream {
    ROGUE_RNG_STREAM_GATHERING = 0,
    ROGUE_RNG_STREAM_REFINEMENT = 1,
    ROGUE_RNG_STREAM_CRAFT_QUALITY = 2,
    ROGUE_RNG_STREAM_ENHANCEMENT = 3,
    ROGUE_RNG_STREAM_COUNT
} RogueRngStream;

void rogue_rng_streams_seed(unsigned int session_seed);
unsigned int rogue_seed_derive(unsigned int session_seed, unsigned int world_chunk, unsigned int player_level, unsigned int stream_id);
unsigned int rogue_rng_next(RogueRngStream stream);

#endif
