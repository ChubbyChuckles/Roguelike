/**
 * @file ai_trace.c
 * @brief Minimal circular trace buffer for recording AI active-path hashes.
 *
 * The trace buffer stores a small, fixed-capacity ring of tick/hash pairs
 * which are useful for exporting deterministic traces or diagnosing
 * per-tick active path evolution. The API is intentionally tiny: init and
 * push operations only.
 *
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 */

#include "ai_trace.h"
#include <string.h>

/**
 * @brief Initialize (zero) a trace buffer.
 *
 * Resets the buffer contents, cursor, and count. Safe to call on an already
 * initialized buffer. NULL pointer is ignored.
 *
 * @param tb Trace buffer to initialize.
 * @details Calls memset(tb, 0, sizeof(*tb)) if tb != NULL.
 */
void rogue_ai_trace_init(RogueAITraceBuffer* tb)
{
    if (!tb)
        return;
    memset(tb, 0, sizeof(*tb));
}

/**
 * @brief Push a tick/hash entry into the circular trace buffer.
 *
 * Appends a new entry at the current cursor position and advances the
 * cursor modulo ROGUE_AI_TRACE_CAP. The count saturates at the buffer
 * capacity. NULL tb is ignored.
 *
 * @param tb Trace buffer to push into.
 * @param tick_index Tick index associated with the entry.
 * @param path_hash Path hash (e.g., active-path hash) to record.
 * @details
 * - Writes to tb->entries[tb->cursor] with tick_index and path_hash.
 * - Advances tb->cursor = (tb->cursor + 1) % ROGUE_AI_TRACE_CAP.
 * - If tb->count < ROGUE_AI_TRACE_CAP, increments tb->count.
 * @note Circular buffer of capacity ROGUE_AI_TRACE_CAP.
 */
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
