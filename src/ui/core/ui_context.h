#pragma once
/* Minimal Phase 1.1-1.3 UI scaffolding */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" { 
#endif

typedef struct RogueUIRect { float x,y,w,h; } RogueUIRect;

typedef struct RogueUIContextConfig {
    int max_nodes;            /* maximum retained nodes */
    unsigned int seed;        /* seed for dedicated UI RNG channel */
    size_t arena_size;        /* transient frame arena size (bytes) */
} RogueUIContextConfig;

typedef struct RogueUITheme {
    uint32_t panel_bg_color;
    uint32_t text_color;
} RogueUITheme;

typedef struct RogueUIRuntimeStats {
    int node_count;
    int draw_calls;
} RogueUIRuntimeStats;

typedef struct RogueUINode {
    RogueUIRect rect;
    const char* text; /* optional */
    uint32_t color;
    int kind; /* 0=panel 1=text */
} RogueUINode;

typedef struct RogueUIContext {
    RogueUITheme theme;
    RogueUIRuntimeStats stats;
    RogueUINode* nodes;
    int node_capacity;
    int node_count;
    unsigned int rng_state;
    int frame_active;
    /* Simulation snapshot separation (read-only view) */
    const void* sim_snapshot;
    size_t sim_snapshot_size;
    /* Transient arena for per-frame allocations (widget text copies, etc.) */
    unsigned char* arena;
    size_t arena_size;
    size_t arena_offset;
    /* Last serialized hash for diff detection */
    uint64_t last_serial_hash;
} RogueUIContext;

int rogue_ui_init(RogueUIContext* ctx, const RogueUIContextConfig* cfg);
void rogue_ui_shutdown(RogueUIContext* ctx);
void rogue_ui_begin(RogueUIContext* ctx, double delta_time_ms);
void rogue_ui_end(RogueUIContext* ctx);

/* Primitive widgets */
int rogue_ui_panel(RogueUIContext* ctx, RogueUIRect r, uint32_t color);
int rogue_ui_text(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color);

/* Accessors */
const RogueUINode* rogue_ui_nodes(const RogueUIContext* ctx, int* count_out);

/* Deterministic RNG (xorshift32) dedicated for cosmetic effects */
unsigned int rogue_ui_rng_next(RogueUIContext* ctx);
void rogue_ui_set_theme(RogueUIContext* ctx, const RogueUITheme* theme);

/* Simulation snapshot interface (UI only reads) */
void rogue_ui_set_simulation_snapshot(RogueUIContext* ctx, const void* snapshot, size_t size);
const void* rogue_ui_simulation_snapshot(const RogueUIContext* ctx, size_t* size_out);

/* Transient arena allocation (resets each frame on begin) */
void* rogue_ui_arena_alloc(RogueUIContext* ctx, size_t size, size_t align);

/* Convenience: create text node duplicating string into arena */
int rogue_ui_text_dup(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color);

/* Snapshot serialization & diff */
/* Serializes current UI tree into deterministic text form (one node per line). */
size_t rogue_ui_serialize(const RogueUIContext* ctx, char* buffer, size_t buffer_size);
/* Computes hash of current tree and compares with last stored; returns 1 if different, updates stored hash. */
int rogue_ui_diff_changed(RogueUIContext* ctx);

#ifdef __cplusplus
}
#endif
