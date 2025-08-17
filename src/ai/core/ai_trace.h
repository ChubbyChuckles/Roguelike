#ifndef ROGUE_AI_TRACE_H
#define ROGUE_AI_TRACE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" { 
#endif

#define ROGUE_AI_TRACE_CAP 64

typedef struct RogueAITraceEntry {
    uint32_t tick_index;    // sequential tick number
    uint32_t hash;          // hash of serialized active path
} RogueAITraceEntry;

typedef struct RogueAITraceBuffer {
    RogueAITraceEntry entries[ROGUE_AI_TRACE_CAP];
    uint16_t count;
    uint16_t cursor; // ring head
} RogueAITraceBuffer;

void rogue_ai_trace_init(RogueAITraceBuffer* tb);
void rogue_ai_trace_push(RogueAITraceBuffer* tb, uint32_t tick_index, uint32_t path_hash);

#ifdef __cplusplus
}
#endif
#endif
