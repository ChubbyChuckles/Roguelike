#ifndef ROGUE_AI_UTILITY_SCORER_H
#define ROGUE_AI_UTILITY_SCORER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" { 
#endif

struct RogueBlackboard;

typedef float (*RogueUtilityScoreFn)(struct RogueBlackboard* bb, void* user_data);

typedef struct RogueUtilityScorer {
    RogueUtilityScoreFn fn;
    void* user_data;
    const char* debug_name;
} RogueUtilityScorer;

static inline float rogue_utility_score(const RogueUtilityScorer* s, struct RogueBlackboard* bb) {
    return s && s->fn ? s->fn(bb, s->user_data) : 0.0f;
}

#ifdef __cplusplus
}
#endif

#endif // ROGUE_AI_UTILITY_SCORER_H
