#pragma once
/* Minimal Phase 1.1-1.3 UI scaffolding */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" { 
#endif

typedef struct RogueUIRect { float x,y,w,h; } RogueUIRect;

typedef struct RogueUIContextConfig {
    int max_nodes;
    unsigned int seed;
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

#ifdef __cplusplus
}
#endif
