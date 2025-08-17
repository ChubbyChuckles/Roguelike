#include "blackboard.h"
#include <string.h>

void rogue_bb_init(RogueBlackboard* bb) {
    if(!bb) return;
    bb->count = 0;
    for(int i=0;i<ROGUE_BB_MAX_ENTRIES;i++) {
        bb->entries[i].key = 0;
        bb->entries[i].type = ROGUE_BB_NONE;
    }
}

static RogueBBEntry* rogue_bb_find(RogueBlackboard* bb, const char* key) {
    if(!bb || !key) return 0;
    for(uint8_t i=0;i<bb->count;i++) {
        if(bb->entries[i].key && strcmp(bb->entries[i].key, key)==0) return &bb->entries[i];
    }
    return 0;
}

static RogueBBEntry* rogue_bb_find_or_add(RogueBlackboard* bb, const char* key) {
    RogueBBEntry* e = rogue_bb_find(bb, key);
    if(e) return e;
    if(bb->count >= ROGUE_BB_MAX_ENTRIES) return 0;
    e = &bb->entries[bb->count++];
    e->key = key; // assume static string lifetime for early phase
    return e;
}

#define BB_SET_BODY(TYPE_ENUM, FIELD, VALUE_EXPR) \
    if(!bb || !key) return false; \
    RogueBBEntry* e = rogue_bb_find_or_add(bb, key); \
    if(!e) return false; \
    e->type = TYPE_ENUM; \
    e->v.FIELD = (VALUE_EXPR); \
    return true;

bool rogue_bb_set_int(RogueBlackboard* bb, const char* key, int value) { BB_SET_BODY(ROGUE_BB_INT, i, value) }
bool rogue_bb_set_float(RogueBlackboard* bb, const char* key, float value) { BB_SET_BODY(ROGUE_BB_FLOAT, f, value) }
bool rogue_bb_set_bool(RogueBlackboard* bb, const char* key, bool value) { BB_SET_BODY(ROGUE_BB_BOOL, b, value) }
bool rogue_bb_set_ptr(RogueBlackboard* bb, const char* key, void* value) { BB_SET_BODY(ROGUE_BB_PTR, p, value) }

#define BB_GET_BODY(TYPE_ENUM, FIELD, OUT_PTR) \
    if(!bb || !key || !OUT_PTR) return false; \
    RogueBBEntry* e = rogue_bb_find((RogueBlackboard*)bb, key); \
    if(!e || e->type != TYPE_ENUM) return false; \
    *OUT_PTR = e->v.FIELD; \
    return true;

bool rogue_bb_get_int(const RogueBlackboard* bb, const char* key, int* out_value) { BB_GET_BODY(ROGUE_BB_INT, i, out_value) }
bool rogue_bb_get_float(const RogueBlackboard* bb, const char* key, float* out_value) { BB_GET_BODY(ROGUE_BB_FLOAT, f, out_value) }
bool rogue_bb_get_bool(const RogueBlackboard* bb, const char* key, bool* out_value) { BB_GET_BODY(ROGUE_BB_BOOL, b, out_value) }
bool rogue_bb_get_ptr(const RogueBlackboard* bb, const char* key, void** out_value) { BB_GET_BODY(ROGUE_BB_PTR, p, out_value) }
