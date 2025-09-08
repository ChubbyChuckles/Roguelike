#ifndef ROGUE_ASSET_BROWSER_STATE_H
#define ROGUE_ASSET_BROWSER_STATE_H

#include <stddef.h>

#ifndef ROGUE_FILE_DIALOG_PATH_MAX
#define ROGUE_FILE_DIALOG_PATH_MAX 260
#endif
#ifndef ROGUE_ASSET_BROWSER_JSON_CAP
#define ROGUE_ASSET_BROWSER_JSON_CAP 256
#endif
#ifndef ROGUE_ASSET_BROWSER_SHADER_CAP
#define ROGUE_ASSET_BROWSER_SHADER_CAP 256
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueAssetBrowserDirEntry
    {
        char name[ROGUE_FILE_DIALOG_PATH_MAX];
        int is_dir;
    } RogueAssetBrowserDirEntry;

    typedef struct AssetBrowserEnhancedState
    {
        int tab_index; /* 0=All,1=Textures,2=Audio,3=JSON,4=Shaders */
        int selected_row;
        char tag_filter[64];
        struct
        {
            char path[260];
        } json_files[ROGUE_ASSET_BROWSER_JSON_CAP];
        int json_count;
        struct
        {
            char path[260];
        } shader_files[ROGUE_ASSET_BROWSER_SHADER_CAP];
        int shader_count;
        int scanned_once;
        char pending_import_path[512];
        int auto_poll_reload;
        int show_stream_queue;
        int show_perf_metrics;
        int show_atlas_tool;
        int show_memory_profiler;
        int show_compression_compare;
        int validation_enabled;
        int validation_last_result;
        int validation_error_count;
        int validation_warning_count;
        char validation_target_path[260];
        char validation_errors[16][96];
        char validation_warnings[16][96];
        int show_optimization;
        int opt_tex_large_count;
        int opt_tex_unloaded_count;
        int opt_audio_unloaded_count;
        char opt_tex_large[8][96];
        char opt_tex_unloaded[8][96];
        char opt_audio_unloaded[8][96];
        int show_cycles;
        int cycle_count;
        int detect_duplicates;
        int duplicate_count;
        char duplicate_records[16][64];
        int show_hotkey_help;
        int show_workflow_templates;
        int show_cache_config;
        int show_vcs_overlay;
        int atlas_selection[8];
        int atlas_selection_count;
        int atlas_last_result;
        unsigned long long approx_texture_bytes;
        size_t mem_total_bytes;
        size_t mem_loaded_bytes;
        int template_counter;
        char last_template_path[260];
        int last_template_result;
        int bookmark_indices[16];
        int bookmark_count;
        char json_editor_buffer[4096];
        int json_editor_dirty;
        char json_undo_stack[8][1024];
        int json_undo_len;
        int json_undo_pos;
        int tex_zoom;
        int pan_x, pan_y;
        char tag_input[64];
        int sprite_grid_show;
        int sprite_grid_cell_w;
        int sprite_grid_cell_h;
        int sprite_edit_mode;
        struct
        {
            int x, y, w, h;
        } sprite_rects[64];
        int sprite_rect_count;
        int sprite_active_rect;
        struct
        {
            int rect_index;
            int duration_ms;
        } anim_frames[64];
        int anim_frame_count;
        int anim_active_frame;
        int audio_loop;
        int audio_volume;
        char json_preview_buffer[4096];
        int json_preview_valid;
        int json_error_count;
        int json_editor_open;
        int json_editor_loaded;
        char json_editor_status[128];
        int json_editor_schema_valid;
        int compare_tex_a;
        int compare_tex_b;
        char dir_cwd[ROGUE_FILE_DIALOG_PATH_MAX];
        char dir_root[ROGUE_FILE_DIALOG_PATH_MAX];
        RogueAssetBrowserDirEntry* dir_entries;
        int dir_count;
        int dir_capacity;
        int dir_scroll;
        int dir_selected;
    } AssetBrowserEnhancedState;

    AssetBrowserEnhancedState* rogue_asset_browser_state(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ASSET_BROWSER_STATE_H */
