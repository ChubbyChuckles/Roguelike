#ifndef ROGUE_AI_BLACKBOARD_H
#define ROGUE_AI_BLACKBOARD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" { 
#endif

typedef enum RogueBBValueType {
    ROGUE_BB_NONE = 0,
    ROGUE_BB_INT,
    ROGUE_BB_FLOAT,
    ROGUE_BB_BOOL,
    ROGUE_BB_PTR,
    ROGUE_BB_VEC2,
    ROGUE_BB_TIMER
} RogueBBValueType;

typedef struct RogueBBVec2 { float x,y; } RogueBBVec2;

typedef enum RogueBBWritePolicy {
    ROGUE_BB_WRITE_SET = 0,
    ROGUE_BB_WRITE_MAX,
    ROGUE_BB_WRITE_MIN,
    ROGUE_BB_WRITE_ACCUM
} RogueBBWritePolicy;

typedef struct RogueBBEntry {
    const char* key;
    RogueBBValueType type;
    union { int i; float f; bool b; void* p; RogueBBVec2 v2; float timer; } v;
    float ttl;          // time-to-live seconds (<=0 means inactive for TTL logic)
    uint8_t dirty;      // dirty flag set when value changes
} RogueBBEntry;

// Very small linear blackboard suitable for early phases.
#define ROGUE_BB_MAX_ENTRIES 32

typedef struct RogueBlackboard {
    RogueBBEntry entries[ROGUE_BB_MAX_ENTRIES];
    uint8_t count;
} RogueBlackboard;

void rogue_bb_init(RogueBlackboard* bb);
bool rogue_bb_set_int(RogueBlackboard* bb, const char* key, int value);
bool rogue_bb_set_float(RogueBlackboard* bb, const char* key, float value);
bool rogue_bb_set_bool(RogueBlackboard* bb, const char* key, bool value);
bool rogue_bb_set_ptr(RogueBlackboard* bb, const char* key, void* value);
bool rogue_bb_set_vec2(RogueBlackboard* bb, const char* key, float x, float y);
bool rogue_bb_set_timer(RogueBlackboard* bb, const char* key, float seconds);
bool rogue_bb_write_int(RogueBlackboard* bb, const char* key, int value, RogueBBWritePolicy policy);
bool rogue_bb_write_float(RogueBlackboard* bb, const char* key, float value, RogueBBWritePolicy policy);
bool rogue_bb_set_ttl(RogueBlackboard* bb, const char* key, float ttl_seconds);
void rogue_bb_tick(RogueBlackboard* bb, float dt); // decay timers & TTL
bool rogue_bb_get_int(const RogueBlackboard* bb, const char* key, int* out_value);
bool rogue_bb_get_float(const RogueBlackboard* bb, const char* key, float* out_value);
bool rogue_bb_get_bool(const RogueBlackboard* bb, const char* key, bool* out_value);
bool rogue_bb_get_ptr(const RogueBlackboard* bb, const char* key, void** out_value);
bool rogue_bb_get_vec2(const RogueBlackboard* bb, const char* key, RogueBBVec2* out_v);
bool rogue_bb_get_timer(const RogueBlackboard* bb, const char* key, float* out_seconds);
bool rogue_bb_is_dirty(const RogueBlackboard* bb, const char* key);
void rogue_bb_clear_dirty(RogueBlackboard* bb, const char* key);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_AI_BLACKBOARD_H
