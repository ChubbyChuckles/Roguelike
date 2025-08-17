#ifndef ROGUE_AI_RNG_H
#define ROGUE_AI_RNG_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" { 
#endif

typedef struct RogueAIRNG {
    uint64_t state;
} RogueAIRNG;

static inline void rogue_ai_rng_seed(RogueAIRNG* r, uint64_t seed){ if(r) r->state = seed?seed:0x9e3779b97f4a7c15ULL; }
static inline uint32_t rogue_ai_rng_next_u32(RogueAIRNG* r){
    // xorshift64*
    uint64_t x = r->state; x ^= x >> 12; x ^= x << 25; x ^= x >> 27; r->state = x; return (uint32_t)((x * 0x2545F4914F6CDD1DULL)>>32);
}
static inline float rogue_ai_rng_next_float(RogueAIRNG* r){ return (rogue_ai_rng_next_u32(r) / 4294967296.0f); }

#ifdef __cplusplus
}
#endif
#endif
