/**
 * @file blackboard.c
 * @brief Simple fixed-size blackboard implementation for AI data sharing.
 *
 * The blackboard stores named entries (int/float/bool/ptr/vec2/timer) in a
 * compact fixed-capacity array suitable for early-phase deterministic tests.
 * Keys are stored as const char* and the implementation assumes static
 * lifetime for keys (see comment at rogue_bb_find_or_add).
 */
#include "blackboard.h"
#include <string.h>

/**
 * @brief Initialize a blackboard to empty state.
 *
 * Sets count to zero and clears all entry metadata (type, ttl, dirty). NULL
 * pointer is ignored.
 *
 * @param bb Blackboard to initialize.
 */
void rogue_bb_init(RogueBlackboard* bb)
{
    if (!bb)
        return;
    bb->count = 0;
    for (int i = 0; i < ROGUE_BB_MAX_ENTRIES; i++)
    {
        bb->entries[i].key = 0;
        bb->entries[i].type = ROGUE_BB_NONE;
        bb->entries[i].ttl = 0.0f;
        bb->entries[i].dirty = 0;
    }
}

/**
 * @brief Find an entry by key.
 *
 * Returns a pointer to the entry or NULL if not found. NULL bb or key
 * results in NULL return.
 *
 * @param bb Blackboard to search.
 * @param key Key string to look up.
 * @return RogueBBEntry* Pointer to found entry or NULL.
 */
static RogueBBEntry* rogue_bb_find(RogueBlackboard* bb, const char* key)
{
    if (!bb || !key)
        return 0;
    for (uint8_t i = 0; i < bb->count; i++)
    {
        if (bb->entries[i].key && strcmp(bb->entries[i].key, key) == 0)
            return &bb->entries[i];
    }
    return 0;
}

/**
 * @brief Find an entry by key or add a new one when missing.
 *
 * If the key is not present and there is capacity, a new entry is created
 * and the key pointer is stored (caller must ensure key string has static
 * lifetime). If capacity is exhausted, NULL is returned.
 *
 * @param bb Blackboard to modify.
 * @param key Key string (stored by pointer, not copied).
 * @return RogueBBEntry* Pointer to existing or newly-added entry, or NULL.
 */
static RogueBBEntry* rogue_bb_find_or_add(RogueBlackboard* bb, const char* key)
{
    RogueBBEntry* e = rogue_bb_find(bb, key);
    if (e)
        return e;
    if (bb->count >= ROGUE_BB_MAX_ENTRIES)
        return 0;
    e = &bb->entries[bb->count++];
    e->key = key; // assume static string lifetime for early phase
    return e;
}

/**
 * @brief Helper macro to implement simple typed setters concisely.
 *
 * Sets the entry type, value, marks it dirty, and returns bool success.
 */
#define BB_SET_BODY(TYPE_ENUM, FIELD, VALUE_EXPR)                                                  \
    if (!bb || !key)                                                                               \
        return false;                                                                              \
    RogueBBEntry* e = rogue_bb_find_or_add(bb, key);                                               \
    if (!e)                                                                                        \
        return false;                                                                              \
    e->type = TYPE_ENUM;                                                                           \
    e->v.FIELD = (VALUE_EXPR);                                                                     \
    e->dirty = 1;                                                                                  \
    return true;

/**
 * @brief Set integer value for key.
 * @param bb Blackboard to modify.
 * @param key Key name (assumed static string lifetime).
 * @param value Integer value to set.
 * @return bool True on success, false on error (OOM or invalid args).
 */
bool rogue_bb_set_int(RogueBlackboard* bb, const char* key, int value)
{
    BB_SET_BODY(ROGUE_BB_INT, i, value)
}
/**
 * @brief Set float value for key.
 */
bool rogue_bb_set_float(RogueBlackboard* bb, const char* key, float value)
{
    BB_SET_BODY(ROGUE_BB_FLOAT, f, value)
}
/**
 * @brief Set boolean value for key.
 */
bool rogue_bb_set_bool(RogueBlackboard* bb, const char* key, bool value)
{
    BB_SET_BODY(ROGUE_BB_BOOL, b, value)
}
/**
 * @brief Set pointer value for key.
 */
bool rogue_bb_set_ptr(RogueBlackboard* bb, const char* key, void* value)
{
    BB_SET_BODY(ROGUE_BB_PTR, p, value)
}
/**
 * @brief Set 2D vector value for key.
 */
bool rogue_bb_set_vec2(RogueBlackboard* bb, const char* key, float x, float y)
{
    if (!bb || !key)
        return false;
    RogueBBEntry* e = rogue_bb_find_or_add(bb, key);
    if (!e)
        return false;
    e->type = ROGUE_BB_VEC2;
    e->v.v2.x = x;
    e->v.v2.y = y;
    e->dirty = 1;
    return true;
}
/**
 * @brief Set timer value (seconds) for key.
 */
bool rogue_bb_set_timer(RogueBlackboard* bb, const char* key, float seconds)
{
    if (!bb || !key)
        return false;
    RogueBBEntry* e = rogue_bb_find_or_add(bb, key);
    if (!e)
        return false;
    e->type = ROGUE_BB_TIMER;
    e->v.timer = seconds;
    e->dirty = 1;
    return true;
}

/**
 * @brief Apply an integer write policy to a destination integer.
 *
 * Policies: set, max, min, accum (accumulate). Returns true when the
 * destination was changed.
 */
static bool apply_policy_int(int* dst, int value, RogueBBWritePolicy policy)
{
    switch (policy)
    {
    case ROGUE_BB_WRITE_SET:
        *dst = value;
        return true;
    case ROGUE_BB_WRITE_MAX:
        if (value > *dst)
        {
            *dst = value;
            return true;
        }
        return false;
    case ROGUE_BB_WRITE_MIN:
        if (value < *dst)
        {
            *dst = value;
            return true;
        }
        return false;
    case ROGUE_BB_WRITE_ACCUM:
        *dst += value;
        return true;
    }
    return false;
}
/**
 * @brief Apply a float write policy to a destination float.
 */
static bool apply_policy_float(float* dst, float value, RogueBBWritePolicy policy)
{
    switch (policy)
    {
    case ROGUE_BB_WRITE_SET:
        *dst = value;
        return true;
    case ROGUE_BB_WRITE_MAX:
        if (value > *dst)
        {
            *dst = value;
            return true;
        }
        return false;
    case ROGUE_BB_WRITE_MIN:
        if (value < *dst)
        {
            *dst = value;
            return true;
        }
        return false;
    case ROGUE_BB_WRITE_ACCUM:
        *dst += value;
        return true;
    }
    return false;
}

/**
 * @brief Write an integer with a write policy (set/max/min/accum).
 *
 * Ensures the entry exists and is of integer type (converting if necessary).
 * Marks the entry dirty if it changed.
 */
bool rogue_bb_write_int(RogueBlackboard* bb, const char* key, int value, RogueBBWritePolicy policy)
{
    if (!bb || !key)
        return false;
    RogueBBEntry* e = rogue_bb_find_or_add(bb, key);
    if (!e)
        return false;
    if (e->type != ROGUE_BB_INT)
    {
        e->type = ROGUE_BB_INT;
        e->v.i = 0;
    }
    if (apply_policy_int(&e->v.i, value, policy))
        e->dirty = 1;
    return true;
}
/**
 * @brief Write a float with a write policy (set/max/min/accum).
 */
bool rogue_bb_write_float(RogueBlackboard* bb, const char* key, float value,
                          RogueBBWritePolicy policy)
{
    if (!bb || !key)
        return false;
    RogueBBEntry* e = rogue_bb_find_or_add(bb, key);
    if (!e)
        return false;
    if (e->type != ROGUE_BB_FLOAT)
    {
        e->type = ROGUE_BB_FLOAT;
        e->v.f = 0.0f;
    }
    if (apply_policy_float(&e->v.f, value, policy))
        e->dirty = 1;
    return true;
}

/**
 * @brief Set a TTL (time-to-live) on an existing key.
 *
 * The TTL will be decremented by rogue_bb_tick and entries whose TTL
 * reaches zero will be cleared (type set to NONE and marked dirty).
 */
bool rogue_bb_set_ttl(RogueBlackboard* bb, const char* key, float ttl_seconds)
{
    if (!bb || !key)
        return false;
    RogueBBEntry* e = rogue_bb_find(bb, key);
    if (!e)
        return false;
    e->ttl = ttl_seconds;
    return true;
}

/**
 * @brief Per-frame tick for blackboard timers and TTLs.
 *
 * Decrements entry TTLs and timers and marks entries dirty when they
 * transition to expired/zero. NULL bb ignored.
 */
void rogue_bb_tick(RogueBlackboard* bb, float dt)
{
    if (!bb)
        return;
    for (uint8_t i = 0; i < bb->count; i++)
    {
        RogueBBEntry* e = &bb->entries[i];
        if (e->ttl > 0.f)
        {
            e->ttl -= dt;
            if (e->ttl <= 0.f)
            {
                e->type = ROGUE_BB_NONE;
                e->dirty = 1;
            }
        }
        if (e->type == ROGUE_BB_TIMER)
        {
            if (e->v.timer > 0.f)
            {
                e->v.timer -= dt;
                if (e->v.timer < 0.f)
                {
                    e->v.timer = 0.f;
                    e->dirty = 1;
                }
            }
        }
    }
}

/**
 * @brief Helper macro to implement typed getters concisely.
 */
#define BB_GET_BODY(TYPE_ENUM, FIELD, OUT_PTR)                                                     \
    if (!bb || !key || !OUT_PTR)                                                                   \
        return false;                                                                              \
    RogueBBEntry* e = rogue_bb_find((RogueBlackboard*) bb, key);                                   \
    if (!e || e->type != TYPE_ENUM)                                                                \
        return false;                                                                              \
    *OUT_PTR = e->v.FIELD;                                                                         \
    return true;

/**
 * @brief Get integer value by key.
 */
bool rogue_bb_get_int(const RogueBlackboard* bb, const char* key, int* out_value)
{
    BB_GET_BODY(ROGUE_BB_INT, i, out_value)
}
/**
 * @brief Get float value by key.
 */
bool rogue_bb_get_float(const RogueBlackboard* bb, const char* key, float* out_value)
{
    BB_GET_BODY(ROGUE_BB_FLOAT, f, out_value)
}
/**
 * @brief Get boolean value by key.
 */
bool rogue_bb_get_bool(const RogueBlackboard* bb, const char* key, bool* out_value)
{
    BB_GET_BODY(ROGUE_BB_BOOL, b, out_value)
}
/**
 * @brief Get pointer value by key.
 */
bool rogue_bb_get_ptr(const RogueBlackboard* bb, const char* key, void** out_value)
{
    BB_GET_BODY(ROGUE_BB_PTR, p, out_value)
}
/**
 * @brief Get Vec2 value by key.
 */
bool rogue_bb_get_vec2(const RogueBlackboard* bb, const char* key, RogueBBVec2* out_v)
{
    BB_GET_BODY(ROGUE_BB_VEC2, v2, out_v)
}
/**
 * @brief Get timer value (seconds) by key.
 */
bool rogue_bb_get_timer(const RogueBlackboard* bb, const char* key, float* out_seconds)
{
    BB_GET_BODY(ROGUE_BB_TIMER, timer, out_seconds)
}
/**
 * @brief Query whether a key is marked dirty.
 *
 * Returns false for missing key or invalid args. Non-zero dirty value is
 * considered true.
 */
bool rogue_bb_is_dirty(const RogueBlackboard* bb, const char* key)
{
    if (!bb || !key)
        return false;
    for (uint8_t i = 0; i < bb->count; i++)
    {
        if (bb->entries[i].key && strcmp(bb->entries[i].key, key) == 0)
            return bb->entries[i].dirty != 0;
    }
    return false;
}
/**
 * @brief Clear the dirty flag for a key.
 */
void rogue_bb_clear_dirty(RogueBlackboard* bb, const char* key)
{
    if (!bb || !key)
        return;
    for (uint8_t i = 0; i < bb->count; i++)
    {
        if (bb->entries[i].key && strcmp(bb->entries[i].key, key) == 0)
        {
            bb->entries[i].dirty = 0;
            return;
        }
    }
}
