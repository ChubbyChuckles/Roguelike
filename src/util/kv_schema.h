/* kv_schema.h - Phase M3.2 Schema + validation error surfacing */
#ifndef ROGUE_UTIL_KV_SCHEMA_H
#define ROGUE_UTIL_KV_SCHEMA_H
#include "util/kv_parser.h"

typedef enum RogueKVType { ROGUE_KV_INT, ROGUE_KV_FLOAT, ROGUE_KV_STRING } RogueKVType;
typedef struct RogueKVFieldDef {
    const char* key; RogueKVType type; int required; /* 1 if required */
} RogueKVFieldDef;
typedef struct RogueKVSchema {
    const RogueKVFieldDef* fields; int field_count; /* sentinel-less */
} RogueKVSchema;

typedef struct RogueKVFieldValue {
    const RogueKVFieldDef* def; int present; long i; double f; const char* s; } RogueKVFieldValue;

/* Validate file content against schema. Returns number of validation errors (0 = success). */
int rogue_kv_validate(const RogueKVFile* file, const RogueKVSchema* schema, RogueKVFieldValue* out_values, int max_values, char* err_buf, int err_buf_sz);

#endif
