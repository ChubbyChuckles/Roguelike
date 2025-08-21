#pragma once
/* Minimal Phase 1.1-1.3 UI scaffolding */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueUIRect
    {
        float x, y, w, h;
    } RogueUIRect;

    typedef struct RogueUIContextConfig
    {
        int max_nodes;     /* maximum retained nodes */
        unsigned int seed; /* seed for dedicated UI RNG channel */
        size_t arena_size; /* transient frame arena size (bytes) */
    } RogueUIContextConfig;

    typedef struct RogueUITheme
    {
        uint32_t panel_bg_color;
        uint32_t text_color;
    } RogueUITheme;

    typedef struct RogueUIRuntimeStats
    {
        int node_count;
        int draw_calls;
    } RogueUIRuntimeStats;

    /* Radial selector descriptor (Phase 4.10) */
    typedef struct RogueUIRadialDesc
    {
        int active;    /* 1 if open */
        int count;     /* number of wedges */
        int selection; /* current highlighted wedge index */
    } RogueUIRadialDesc;

    typedef struct RogueUINode
    {
        RogueUIRect rect;
        const char* text; /* optional */
        uint32_t color;
        uint32_t aux_color; /* secondary color (fill, etc.) */
        float value;        /* generic numeric (progress current) */
        float value_max;    /* generic numeric max */
        int data_i0;        /* sprite sheet id / orientation */
        int data_i1;        /* sprite frame / reserved */
        int kind;           /* 0=panel 1=text 2=image 3=sprite 4=progress 5=button 6=toggle 7=slider
                               8=textinput */
        int parent_index;   /* index of parent container (-1 if root) */
        uint32_t id_hash;   /* stable id hash (label + parent path) */
    } RogueUINode;

    typedef struct RogueUIInputState
    {
        float mouse_x, mouse_y;
        int mouse_down;      /* 1 if held */
        int mouse_pressed;   /* 1 on press this frame */
        int mouse_released;  /* 1 on release this frame */
        int mouse2_pressed;  /* secondary button (right) press this frame */
        int mouse2_released; /* secondary button release this frame */
        float wheel_delta;   /* positive = scroll up, negative = down (per frame aggregated) */
        char text_char;      /* printable character input (ASCII) or 0 */
        char key_char;       /* raw key character (for chords, not inserted into text) */
        int backspace;       /* 1 if backspace pressed this frame */
        int key_tab;
        int key_left, key_right, key_up, key_down;
        int key_activate; /* generic activation (Enter / controller A) */
        int key_ctrl;     /* control modifier held */
        int key_paste;    /* paste shortcut (Ctrl+V mapped by platform layer) */
    } RogueUIInputState;

    typedef struct RogueUIControllerState
    {
        float axis_x; /* -1..1 */
        float axis_y; /* -1..1 */
        int button_a; /* primary */
    } RogueUIControllerState;

    typedef struct RogueUIContext
    {
        RogueUITheme theme;
        int initialized_flag; /* set to 0xC0DEFACE after successful init, zeroed on shutdown to make
                                 idempotent */
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
        int hot_index;    /* widget index under cursor */
        int active_index; /* widget currently active (pressed) */
        int focus_index;  /* focused text input widget */
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
        struct
        {
            char k1;
            char k2;
            int command_id;
        } chord_commands[8];
        int chord_count;
        char pending_chord;
        double pending_chord_time_ms;
        double chord_timeout_ms;
        int last_command_executed;
        /* Input replay */
        int replay_recording;
        int replay_playing;
        int replay_count;
        int replay_cursor;
        RogueUIInputState replay_buffer[512];
        /* Phase 4.2 Drag & Drop */
        int drag_active;
        int drag_from_slot;
        int drag_item_id;
        int drag_item_count;
        /* Phase 4.3 Stack Split */
        int stack_split_active;
        int stack_split_from_slot;
        int stack_split_total;
        int stack_split_value;
        /* Phase 4 event queue */
        struct
        {
            int kind;
            int a;
            int b;
            int c;
        } event_queue[32];
        int event_head;
        int event_tail;
        /* Phase 4.4 Context Menu */
        int ctx_menu_active;
        int ctx_menu_slot;
        int ctx_menu_selection;
        /* Phase 4.5 Inline Stat Delta Preview */
        int stat_preview_slot; /* hovered slot last frame for which preview was generated (-1 if
                                  none) */
        /* Phase 4.10 Radial selector */
        RogueUIRadialDesc radial;
        /* Phase 5 Skill Graph (zoomable, panning, quadtree culling) */
        int skillgraph_active;
        float skillgraph_view_x, skillgraph_view_y;   /* top-left world coords */
        float skillgraph_view_w, skillgraph_view_h;   /* viewport size in world units */
        float skillgraph_zoom;                        /* scale factor (1=100%) */
        struct RogueUISkillNodeRec* skillgraph_nodes; /* dynamic array (heap) */
        int skillgraph_node_count;
        int skillgraph_node_capacity;
        void* skillgraph_quadtree; /* opaque pointer (rebuilt each build) */
        /* Phase 5.3 Animation state */
        struct
        {
            int icon_id;
            float remaining_ms;
        } skillgraph_pulses[32];
        int skillgraph_pulse_count;
        struct
        {
            int icon_id;
            float remaining_ms;
            float y_offset;
            int amount;
        } skillgraph_spends[32];
        int skillgraph_spend_count;
        /* Phase 5.4+ additions */
        int skillgraph_synergy_panel_enabled; /* show aggregate synergy stats panel */
        unsigned int skillgraph_filter_tags;  /* bitmask: if non-zero, only nodes with (tags &
                                                 mask)!=0 are shown */
        /* Phase 5.7 undo buffer */
        struct
        {
            int icon_id;
            int prev_rank;
        } skillgraph_undo[64];
        int skillgraph_undo_count;
        /* Phase 7.5 Reduced Motion */
        int reduced_motion;
        /* Phase 7.6 Narration stub */
        char narration_last[256];
        /* Phase 7.7 Focus audit */
        int focus_audit_enabled;
        /* Phase 8 animation time scale (global UI time dilation) */
        float anim_time_scale;
        /* Phase 9 perf fields */
        double perf_budget_ms;
        double perf_last_frame_ms;
        double perf_last_update_ms;
        double perf_last_render_ms;
        double perf_frame_start_ms;
        double perf_update_start_ms;
        double (*perf_now)(void*);
        void* perf_now_user;
        int prev_node_count;
        int dirty_changed;
        float dirty_x, dirty_y, dirty_w, dirty_h;
        int dirty_node_count;
        int dirty_reported_this_frame;
        int dirty_kind; /* 0=none 1=structural 2=content */
        /* Per-phase timings (ms) */
        double perf_phase_accum[8];
        double perf_phase_start[8];
        /* Regression guard */
        double perf_baseline_ms;           /* baseline frame time */
        double perf_regress_threshold_pct; /* e.g., 0.25 => 25% slower triggers */
        int perf_regressed_flag;           /* latched until baseline reset */
        double perf_autob_samples[64];
        int perf_autob_count;
        /* Glyph cache (simple) */
        struct RogueUIGlyphEntry
        {
            unsigned int codepoint;
            float advance;
            unsigned int lru_tick;
        }* glyph_cache;
        int glyph_cache_count;
        int glyph_cache_capacity;
        unsigned int glyph_cache_tick;
        int glyph_cache_hits;
        int glyph_cache_misses;
        /* Phase 11.2 Inspector */
        int inspector_enabled;
        int inspector_selected_index;
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
    int rogue_ui_progress_bar(RogueUIContext* ctx, RogueUIRect r, float value, float max_value,
                              uint32_t bg_color, uint32_t fill_color, int orientation /*0=H,1=V*/);

    /* Interactive widgets (Phase 2.2) */
    int rogue_ui_button(RogueUIContext* ctx, RogueUIRect r, const char* label, uint32_t bg_color,
                        uint32_t text_color);
    int rogue_ui_toggle(RogueUIContext* ctx, RogueUIRect r, const char* label, int* state,
                        uint32_t off_color, uint32_t on_color, uint32_t text_color);
    int rogue_ui_slider(RogueUIContext* ctx, RogueUIRect r, float min_v, float max_v, float* value,
                        uint32_t track_color, uint32_t fill_color);
    int rogue_ui_text_input(RogueUIContext* ctx, RogueUIRect r, char* buffer, int buffer_cap,
                            uint32_t bg_color, uint32_t text_color);

    /* Layout (Phase 2.3) */
    int rogue_ui_row_begin(RogueUIContext* ctx, RogueUIRect r, int padding, int spacing);
    int rogue_ui_column_begin(RogueUIContext* ctx, RogueUIRect r, int padding, int spacing);
    /* Returns next child rect inside row/column advancing cursor */
    int rogue_ui_row_next(RogueUIContext* ctx, int row_index, float width, float height,
                          RogueUIRect* out_rect);
    int rogue_ui_column_next(RogueUIContext* ctx, int col_index, float width, float height,
                             RogueUIRect* out_rect);
    /* Grid helper: compute cell rect; does not push node (user passes result to widget call) */
    RogueUIRect rogue_ui_grid_cell(RogueUIRect grid_rect, int rows, int cols, int r, int c,
                                   int padding, int spacing);
    /* Layer stack: push layer container (data_i0 = layer order) */
    int rogue_ui_layer(RogueUIContext* ctx, RogueUIRect r, int layer_order);

    /* Scroll Container (Phase 2.4) */
    int rogue_ui_scroll_begin(RogueUIContext* ctx, RogueUIRect r, float content_height);
    void rogue_ui_scroll_set_content(int scroll_index, RogueUIContext* ctx, float content_height);
    float rogue_ui_scroll_offset(const RogueUIContext* ctx, int scroll_index);
    /* Apply vertical scroll offset to child rectangle (helper) */
    RogueUIRect rogue_ui_scroll_apply(const RogueUIContext* ctx, int scroll_index,
                                      RogueUIRect child_raw);

    /* Tooltip (Phase 2.5) */
    int rogue_ui_tooltip(RogueUIContext* ctx, int target_index, const char* text, uint32_t bg_color,
                         uint32_t text_color, int delay_ms);

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
    void rogue_ui_key_repeat_config(RogueUIContext* ctx, double initial_delay_ms,
                                    double interval_ms);

    /* Chorded shortcuts (Phase 3.5) */
    int rogue_ui_register_chord(RogueUIContext* ctx, char k1, char k2, int command_id);
    int rogue_ui_last_command(const RogueUIContext* ctx);

    /* Input replay (Phase 3.6) */
    void rogue_ui_replay_start_record(RogueUIContext* ctx);
    void rogue_ui_replay_stop_record(RogueUIContext* ctx);
    void rogue_ui_replay_start_playback(RogueUIContext* ctx);
    int rogue_ui_replay_step(RogueUIContext* ctx); /* returns 0 when finished */

    /* Phase 7.5 Reduced Motion Toggle */
    void rogue_ui_set_reduced_motion(RogueUIContext* ctx, int enabled);
    int rogue_ui_reduced_motion(const RogueUIContext* ctx);

    /* Phase 7.6 Screen reader / narration stub */
    void rogue_ui_narrate(RogueUIContext* ctx, const char* text);
    const char* rogue_ui_last_narration(const RogueUIContext* ctx);

    /* Phase 7.7 Focus audit: enable & emit overlay highlight panels around focusable widgets */
    void rogue_ui_focus_audit_enable(RogueUIContext* ctx, int enabled);
    int rogue_ui_focus_audit_enabled(const RogueUIContext* ctx);
    int rogue_ui_focus_audit_emit_overlays(RogueUIContext* ctx, uint32_t highlight_color);
    /* Export tab/focus order into buffer as newline separated labels (or kind index placeholder) */
    size_t rogue_ui_focus_order_export(RogueUIContext* ctx, char* buffer, size_t cap);

    /* Phase 8 Animation & Transitions */
    void rogue_ui_set_time_scale(RogueUIContext* ctx, float scale); /* Phase 8.5 */
    /* Easing types */
    typedef enum RogueUIEaseType
    {
        ROGUE_EASE_LINEAR = 0,
        ROGUE_EASE_CUBIC_IN,
        ROGUE_EASE_CUBIC_OUT,
        ROGUE_EASE_CUBIC_IN_OUT,
        ROGUE_EASE_SPRING,
        ROGUE_EASE_ELASTIC_OUT
    } RogueUIEaseType;
    float rogue_ui_ease(RogueUIEaseType t, float x); /* Phase 8.1 */
    /* Entrance / Exit transitions (scale+alpha) Phase 8.3 */
    void rogue_ui_entrance(RogueUIContext* ctx, uint32_t id_hash, float duration_ms,
                           RogueUIEaseType ease);
    void rogue_ui_exit(RogueUIContext* ctx, uint32_t id_hash, float duration_ms,
                       RogueUIEaseType ease);
    /* Micro-interaction spring pulse (button press) Phase 8.4 */
    void rogue_ui_button_press_pulse(RogueUIContext* ctx, uint32_t id_hash);
    /* Query current animated scale/alpha (1.0 / 1.0 if none) */
    float rogue_ui_anim_scale(const RogueUIContext* ctx, uint32_t id_hash);
    float rogue_ui_anim_alpha(const RogueUIContext* ctx, uint32_t id_hash);

    /* Phase 9 Performance & Virtualization */
    typedef struct RogueUIDirtyInfo
    {
        int changed;
        float x, y, w, h;
        int changed_node_count;
        int kind;
    } RogueUIDirtyInfo; /* union of changed rects + kind classification */
    /* Virtualized list query helper (no nodes emitted, just computes visible range) */
    int rogue_ui_list_virtual_range(int total_items, int item_height, int view_height,
                                    int scroll_offset, int* first_index_out, int* count_out);
    /* Optional helper that emits panels for visible items (simple) */
    int rogue_ui_list_virtual_emit(RogueUIContext* ctx, RogueUIRect area, int total_items,
                                   int item_height, int scroll_offset, uint32_t color_base,
                                   uint32_t color_alt);
    /* Dirty rectangle info for last frame begin/end compared to previous */
    RogueUIDirtyInfo rogue_ui_dirty_info(const RogueUIContext* ctx);
    /* Extended dirty classification (Phase 9.2) kind: 0=none 1=structural (tree size/layout)
     * 2=content (text/color diff) */
    /* NOTE: kind is exposed via RogueUIDirtyInfo.kind after Phase 9.2 implementation */
    /* Performance instrumentation */
    void rogue_ui_perf_set_budget(RogueUIContext* ctx, double frame_budget_ms);
    int rogue_ui_perf_frame_over_budget(const RogueUIContext* ctx);
    double rogue_ui_perf_last_update_ms(const RogueUIContext* ctx);
    double rogue_ui_perf_last_render_ms(const RogueUIContext* ctx);
    /* Per-phase CPU timings (Phase 9.1). phase_id 0=update/build 1=render 2=animation(optional)
     * reserved others */
    void rogue_ui_perf_phase_begin(RogueUIContext* ctx, int phase_id);
    void rogue_ui_perf_phase_end(RogueUIContext* ctx, int phase_id);
    double rogue_ui_perf_phase_ms(const RogueUIContext* ctx, int phase_id);
    /* Inject custom clock for deterministic tests */
    void rogue_ui_perf_set_time_provider(RogueUIContext* ctx, double (*now_ms_fn)(void*),
                                         void* user);
    /* Simulated render phase (records timing + dirty logic finalize) */
    void rogue_ui_render(RogueUIContext* ctx);
    /* Frame time regression guardrails (Phase 9.5) */
    void rogue_ui_perf_set_baseline(RogueUIContext* ctx, double baseline_ms);
    void rogue_ui_perf_set_regression_threshold(RogueUIContext* ctx, double pct_over_baseline);
    int rogue_ui_perf_regressed(const RogueUIContext* ctx);
    /* Optionally accumulate N frames then set baseline to their average */
    void rogue_ui_perf_auto_baseline_reset(RogueUIContext* ctx);
    void rogue_ui_perf_auto_baseline_add_sample(RogueUIContext* ctx, double frame_ms,
                                                int target_count);

    /* Text shaping / glyph cache (Phase 9.3) */
    /* Simplified glyph cache storing per-codepoint advance width. For tests we synthesize width
     * from codepoint. */
    void rogue_ui_text_cache_reset(RogueUIContext* ctx);
    float rogue_ui_text_cache_measure(RogueUIContext* ctx, const char* text);
    int rogue_ui_text_cache_hits(const RogueUIContext* ctx);
    int rogue_ui_text_cache_misses(const RogueUIContext* ctx);
    int rogue_ui_text_cache_size(const RogueUIContext* ctx);
    void rogue_ui_text_cache_compact(
        RogueUIContext* ctx); /* removes least-recently used half if over soft cap */

/* Declarative Widget DSL (Phase 2.6) */
#define UI_PANEL(ctx, X, Y, W, H, COLOR)                                                           \
    rogue_ui_panel((ctx), (RogueUIRect){(X), (Y), (W), (H)}, (COLOR))
#define UI_TEXT(ctx, X, Y, W, H, TEXT, COLOR)                                                      \
    rogue_ui_text((ctx), (RogueUIRect){(X), (Y), (W), (H)}, (TEXT), (COLOR))
#define UI_BUTTON(ctx, X, Y, W, H, LABEL, BG, FG)                                                  \
    rogue_ui_button((ctx), (RogueUIRect){(X), (Y), (W), (H)}, (LABEL), (BG), (FG))

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
    /* Computes hash of current tree and compares with last stored; returns 1 if different, updates
     * stored hash. */
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
    /* Inventory grid (Phase 4.1-4.3) now supports drag & drop and stack split (CTRL+click). */
    int rogue_ui_inventory_grid(RogueUIContext* ctx, RogueUIRect rect, const char* id,
                                int slot_capacity, int columns, int* item_ids, int* item_counts,
                                int cell_size, int* first_visible, int* visible_count);

    /* Phase 10.1 Headless Harness */
    /* Build callback invoked between begin/end inside headless runner. */
    typedef void (*RogueUIBuildFn)(RogueUIContext* ctx, void* user);
    /* Runs a single deterministic headless frame (delta_time_ms supplied) returning serialized
     * hash. Returns 1 on success. */
    int rogue_ui_headless_run(const RogueUIContextConfig* cfg, double delta_time_ms,
                              RogueUIBuildFn build, void* user, uint64_t* out_hash);
    /* Direct helper: compute current tree hash (same algorithm used by diff) without mutating diff
     * state. */
    uint64_t rogue_ui_tree_hash(RogueUIContext* ctx);

    /* Phase 11.1 Style Guide Catalog */
    void rogue_ui_style_guide_build(RogueUIContext* ctx);

    /* Phase 11.2 Developer Hierarchy Inspector */
    void rogue_ui_inspector_enable(RogueUIContext* ctx, int enabled);
    int rogue_ui_inspector_enabled(const RogueUIContext* ctx);
    void rogue_ui_inspector_select(RogueUIContext* ctx, int node_index);
    int rogue_ui_inspector_emit(RogueUIContext* ctx,
                                uint32_t highlight_color); /* returns overlay node index or -1 */
    int rogue_ui_inspector_edit_color(RogueUIContext* ctx, int node_index, uint32_t new_color);

    /* Phase 11.3 Crash Snapshot */
    typedef struct RogueUICrashSnapshot
    {
        int node_count;
        uint64_t tree_hash;
        RogueUIInputState input;
    } RogueUICrashSnapshot;
    int rogue_ui_snapshot(const RogueUIContext* ctx, RogueUICrashSnapshot* out);

    /* Phase 5 Skill Graph API */
    /* Begin a skill graph frame section: defines viewport in world coordinates and zoom scale. */
    void rogue_ui_skillgraph_begin(RogueUIContext* ctx, float view_x, float view_y, float view_w,
                                   float view_h, float zoom);
    /* Add a skill node (world coordinate center) with icon id, rank, max_rank, and synergy flag
     * (non-zero displays glow). */
    /* Add a skill node. 'tags' is a bitmask (Phase 5.5 filtering). */
    void rogue_ui_skillgraph_add(RogueUIContext* ctx, float world_x, float world_y, int icon_id,
                                 int rank, int max_rank, int synergy, unsigned int tags);
    /* Build (emit UI nodes) with quadtree culling. Returns number of visible skill nodes emitted
     * (base icons counted once). */
    int rogue_ui_skillgraph_build(RogueUIContext* ctx);
    /* Phase 5.3: Trigger a rank pulse animation (brief highlight/panel overlay). */
    void rogue_ui_skillgraph_pulse(RogueUIContext* ctx, int icon_id);
    /* Phase 5.3: Spawn a currency spend flyout (e.g., talent point cost). */
    void rogue_ui_skillgraph_spend_flyout(RogueUIContext* ctx, int icon_id, int amount);
    /* Phase 5.4: enable/disable passive synergy aggregate panel */
    void rogue_ui_skillgraph_enable_synergy_panel(RogueUIContext* ctx, int enable);
    /* Phase 5.5: set active tag filter (0 clears filter) */
    void rogue_ui_skillgraph_set_filter_tags(RogueUIContext* ctx, unsigned int tag_mask);
    /* Phase 5.6: export/import build (ranks) */
    size_t rogue_ui_skillgraph_export(const RogueUIContext* ctx, char* buffer, size_t cap);
    int rogue_ui_skillgraph_import(RogueUIContext* ctx, const char* buffer);
    /* Phase 5.7: allocate a rank point & record undo (returns 1 if applied) */
    int rogue_ui_skillgraph_allocate(RogueUIContext* ctx, int icon_id);
    /* Undo last allocation (returns 1 if undone) */
    int rogue_ui_skillgraph_undo(RogueUIContext* ctx);

    /* Tag bitmasks (example set for tests) */
    enum
    {
        ROGUE_UI_SKILL_TAG_FIRE = 1 << 0,
        ROGUE_UI_SKILL_TAG_MOVEMENT = 1 << 1,
        ROGUE_UI_SKILL_TAG_DEFENSE = 1 << 2
    };

    /* Phase 4 event kinds */
    enum
    {
        ROGUE_UI_EVENT_NONE = 0,
        ROGUE_UI_EVENT_DRAG_BEGIN = 1,
        ROGUE_UI_EVENT_DRAG_END = 2,
        ROGUE_UI_EVENT_STACK_SPLIT_OPEN = 3,
        ROGUE_UI_EVENT_STACK_SPLIT_APPLY = 4,
        ROGUE_UI_EVENT_STACK_SPLIT_CANCEL = 5,
        ROGUE_UI_EVENT_CONTEXT_OPEN = 6,
        ROGUE_UI_EVENT_CONTEXT_SELECT = 7,
        ROGUE_UI_EVENT_CONTEXT_CANCEL = 8,
        ROGUE_UI_EVENT_STAT_PREVIEW_SHOW = 9,
        ROGUE_UI_EVENT_STAT_PREVIEW_HIDE = 10,
        /* Phase 4.8 (Transaction confirmation) */
        ROGUE_UI_EVENT_VENDOR_CONFIRM_OPEN = 11,
        ROGUE_UI_EVENT_VENDOR_CONFIRM_ACCEPT = 12,
        ROGUE_UI_EVENT_VENDOR_CONFIRM_CANCEL = 13,
        ROGUE_UI_EVENT_VENDOR_INSUFFICIENT_FUNDS = 14,
        /* Phase 4.10 Radial selector */
        ROGUE_UI_EVENT_RADIAL_OPEN = 15,
        ROGUE_UI_EVENT_RADIAL_CHOOSE = 16,
        ROGUE_UI_EVENT_RADIAL_CANCEL = 17
    };

    /* Convenience API for radial selector (controller friendly) */
    void rogue_ui_radial_open(RogueUIContext* ctx, int count);
    void rogue_ui_radial_close(RogueUIContext* ctx); /* emits CANCEL if still active */
    /* Build radial menu widgets (panels + labels) centered at (cx,cy). Returns root index of first
     * node created or -1. */
    int rogue_ui_radial_menu(RogueUIContext* ctx, float cx, float cy, float radius,
                             const char** labels, int count);
    typedef struct RogueUIEvent
    {
        int kind;
        int a;
        int b;
        int c;
    } RogueUIEvent;
    int rogue_ui_poll_event(RogueUIContext* ctx, RogueUIEvent* out);

#ifdef __cplusplus
}
#endif
