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
    uint32_t aux_color; /* secondary color (fill, etc.) */
    float value;        /* generic numeric (progress current) */
    float value_max;    /* generic numeric max */
    int data_i0;        /* sprite sheet id / orientation */
    int data_i1;        /* sprite frame / reserved */
    int kind; /* 0=panel 1=text 2=image 3=sprite 4=progress 5=button 6=toggle 7=slider 8=textinput */
    int parent_index;   /* index of parent container (-1 if root) */
    uint32_t id_hash;   /* stable id hash (label + parent path) */
} RogueUINode;

typedef struct RogueUIInputState {
    float mouse_x, mouse_y;
    int mouse_down;     /* 1 if held */
    int mouse_pressed;  /* 1 on press this frame */
    int mouse_released; /* 1 on release this frame */
    float wheel_delta;  /* positive = scroll up, negative = down (per frame aggregated) */
    char text_char;     /* printable character input (ASCII) or 0 */
    char key_char;      /* raw key character (for chords, not inserted into text) */
    int backspace;      /* 1 if backspace pressed this frame */
    int key_tab;
    int key_left, key_right, key_up, key_down;
    int key_activate;   /* generic activation (Enter / controller A) */
    int key_ctrl;       /* control modifier held */
    int key_paste;      /* paste shortcut (Ctrl+V mapped by platform layer) */
} RogueUIInputState;

typedef struct RogueUIControllerState {
    float axis_x; /* -1..1 */
    float axis_y; /* -1..1 */
    int button_a; /* primary */
} RogueUIControllerState;

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
    /* Simple immediate-mode interaction state (pre-ID system) */
    RogueUIInputState input;
    int hot_index;      /* widget index under cursor */
    int active_index;   /* widget currently active (pressed) */
    int focus_index;    /* focused text input widget */
    /* Time tracking */
    double frame_dt_ms;
    double time_ms;
    /* Tooltip hover tracking */
    double last_hover_start_ms;
    int last_hover_index;
    /* Modal routing */
    int modal_index; /* >=0 if modal root is active */
    /* Controller mapping */
    RogueUIControllerState controller;
    /* Key repeat timing */
    double key_repeat_accum[8];
    int key_repeat_state[8];
    double key_repeat_initial_ms;
    double key_repeat_interval_ms;
    /* Chorded shortcuts (two-step) */
    struct { char k1; char k2; int command_id; } chord_commands[8];
    int chord_count; char pending_chord; double pending_chord_time_ms; double chord_timeout_ms; int last_command_executed;
    /* Input replay */
    int replay_recording; int replay_playing; int replay_count; int replay_cursor; RogueUIInputState replay_buffer[512];
} RogueUIContext;

int rogue_ui_init(RogueUIContext* ctx, const RogueUIContextConfig* cfg);
void rogue_ui_shutdown(RogueUIContext* ctx);
void rogue_ui_begin(RogueUIContext* ctx, double delta_time_ms);
void rogue_ui_end(RogueUIContext* ctx);

/* Primitive widgets */
int rogue_ui_panel(RogueUIContext* ctx, RogueUIRect r, uint32_t color);
int rogue_ui_text(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color);
int rogue_ui_image(RogueUIContext* ctx, RogueUIRect r, const char* path, uint32_t tint);
int rogue_ui_sprite(RogueUIContext* ctx, RogueUIRect r, int sheet_id, int frame, uint32_t tint);
int rogue_ui_progress_bar(RogueUIContext* ctx, RogueUIRect r, float value, float max_value, uint32_t bg_color, uint32_t fill_color, int orientation /*0=H,1=V*/);

/* Interactive widgets (Phase 2.2) */
int rogue_ui_button(RogueUIContext* ctx, RogueUIRect r, const char* label, uint32_t bg_color, uint32_t text_color);
int rogue_ui_toggle(RogueUIContext* ctx, RogueUIRect r, const char* label, int* state, uint32_t off_color, uint32_t on_color, uint32_t text_color);
int rogue_ui_slider(RogueUIContext* ctx, RogueUIRect r, float min_v, float max_v, float* value, uint32_t track_color, uint32_t fill_color);
int rogue_ui_text_input(RogueUIContext* ctx, RogueUIRect r, char* buffer, int buffer_cap, uint32_t bg_color, uint32_t text_color);

/* Layout (Phase 2.3) */
int rogue_ui_row_begin(RogueUIContext* ctx, RogueUIRect r, int padding, int spacing);
int rogue_ui_column_begin(RogueUIContext* ctx, RogueUIRect r, int padding, int spacing);
/* Returns next child rect inside row/column advancing cursor */
int rogue_ui_row_next(RogueUIContext* ctx, int row_index, float width, float height, RogueUIRect* out_rect);
int rogue_ui_column_next(RogueUIContext* ctx, int col_index, float width, float height, RogueUIRect* out_rect);
/* Grid helper: compute cell rect; does not push node (user passes result to widget call) */
RogueUIRect rogue_ui_grid_cell(RogueUIRect grid_rect, int rows, int cols, int r, int c, int padding, int spacing);
/* Layer stack: push layer container (data_i0 = layer order) */
int rogue_ui_layer(RogueUIContext* ctx, RogueUIRect r, int layer_order);

/* Scroll Container (Phase 2.4) */
int rogue_ui_scroll_begin(RogueUIContext* ctx, RogueUIRect r, float content_height);
void rogue_ui_scroll_set_content(int scroll_index, RogueUIContext* ctx, float content_height);
float rogue_ui_scroll_offset(const RogueUIContext* ctx, int scroll_index);
/* Apply vertical scroll offset to child rectangle (helper) */
RogueUIRect rogue_ui_scroll_apply(const RogueUIContext* ctx, int scroll_index, RogueUIRect child_raw);

/* Tooltip (Phase 2.5) */
int rogue_ui_tooltip(RogueUIContext* ctx, int target_index, const char* text, uint32_t bg_color, uint32_t text_color, int delay_ms);

/* Focus & Navigation (Phase 2.8) */
void rogue_ui_navigation_update(RogueUIContext* ctx);

/* Modal routing (Phase 3.1) */
void rogue_ui_set_modal(RogueUIContext* ctx, int modal_index);

/* Controller input (Phase 3.2) */
void rogue_ui_set_controller(RogueUIContext* ctx, const RogueUIControllerState* st);

/* Clipboard & IME stubs (Phase 3.3) */
void rogue_ui_clipboard_set(const char* text);
const char* rogue_ui_clipboard_get(void);
void rogue_ui_ime_start(void);
void rogue_ui_ime_cancel(void);
void rogue_ui_ime_commit(RogueUIContext* ctx, const char* text);

/* Key repeat config (Phase 3.4) */
void rogue_ui_key_repeat_config(RogueUIContext* ctx, double initial_delay_ms, double interval_ms);

/* Chorded shortcuts (Phase 3.5) */
int rogue_ui_register_chord(RogueUIContext* ctx, char k1, char k2, int command_id);
int rogue_ui_last_command(const RogueUIContext* ctx);

/* Input replay (Phase 3.6) */
void rogue_ui_replay_start_record(RogueUIContext* ctx);
void rogue_ui_replay_stop_record(RogueUIContext* ctx);
void rogue_ui_replay_start_playback(RogueUIContext* ctx);
int rogue_ui_replay_step(RogueUIContext* ctx); /* returns 0 when finished */

/* Declarative Widget DSL (Phase 2.6) */
#define UI_PANEL(ctx,X,Y,W,H,COLOR)      rogue_ui_panel((ctx),(RogueUIRect){(X),(Y),(W),(H)},(COLOR))
#define UI_TEXT(ctx,X,Y,W,H,TEXT,COLOR)  rogue_ui_text((ctx),(RogueUIRect){(X),(Y),(W),(H)},(TEXT),(COLOR))
#define UI_BUTTON(ctx,X,Y,W,H,LABEL,BG,FG) rogue_ui_button((ctx),(RogueUIRect){(X),(Y),(W),(H)},(LABEL),(BG),(FG))

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

/* Input injection (call before building widgets each frame) */
void rogue_ui_set_input(RogueUIContext* ctx, const RogueUIInputState* in);

/* Query focus */
int rogue_ui_focused_index(const RogueUIContext* ctx);

/* ID hashing (Phase 2.7) */
uint32_t rogue_ui_make_id(const char* label);
const RogueUINode* rogue_ui_find_by_id(const RogueUIContext* ctx, uint32_t id_hash);

/* Phase 4 (Incremental): Minimal inventory grid helper.
    Emits one panel per slot (and optional text node for item count) inside a container panel.
    Scrolling, dragging, and stack splitting deferred to later increments. */
int rogue_ui_inventory_grid(RogueUIContext* ctx, RogueUIRect rect, const char* id,
     int slot_capacity, int columns, const int* item_ids, const int* item_counts,
     int cell_size, int* first_visible, int* visible_count);

#ifdef __cplusplus
}
#endif
