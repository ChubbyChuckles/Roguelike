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
    ROGUE_BB_PTR
} RogueBBValueType;

typedef struct RogueBBEntry {
    const char* key;
    RogueBBValueType type;
    union { int i; float f; bool b; void* p; } v;
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
bool rogue_bb_get_int(const RogueBlackboard* bb, const char* key, int* out_value);
bool rogue_bb_get_float(const RogueBlackboard* bb, const char* key, float* out_value);
bool rogue_bb_get_bool(const RogueBlackboard* bb, const char* key, bool* out_value);
bool rogue_bb_get_ptr(const RogueBlackboard* bb, const char* key, void** out_value);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_AI_BLACKBOARD_H
