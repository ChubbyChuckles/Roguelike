#include "ai_trace.h"
#include <string.h>

void rogue_ai_trace_init(RogueAITraceBuffer* tb)
{
    if (!tb)
        return;
    memset(tb, 0, sizeof(*tb));
}

void rogue_ai_trace_push(RogueAITraceBuffer* tb, uint32_t tick_index, uint32_t path_hash)
{
    if (!tb)
        return;
    RogueAITraceEntry* e = &tb->entries[tb->cursor];
    e->tick_index = tick_index;
    e->hash = path_hash;
    tb->cursor = (tb->cursor + 1) % ROGUE_AI_TRACE_CAP;
    if (tb->count < ROGUE_AI_TRACE_CAP)
        tb->count++;
}
