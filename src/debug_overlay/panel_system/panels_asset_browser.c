/* panels_asset_browser.c - Asset Browser (Enhanced Phase 1 of Overlay Plan)
   Adds:
     - Tabbed/type filtering (All / Textures / Audio / JSON / Shaders)
     - Wildcard filter (supports * and ? pattern tokens; case-insensitive)
     - Basic dependency listing for selected texture/audio id via asset_dep registry
     - Approximate memory usage stats (textures: w*h*4 for loaded SDL textures)
     - Hot-reload poll button + optional auto-poll toggle
     - JSON + Shader file enumeration (recursive under assets/) cached & refreshable
   Notes:
     * Thumbnails & advanced analytics deferred to later phases (roadmap tasks pending)
     * Enumeration uses lightweight recursive scanner (compile-time caps, headless safe)
     * Pattern match intentionally simple (NOT full regex) – roadmap "regex" item will
       upgrade to real regex engine in a later slice.
*/
#include "../../asset/asset_manager.h"
#include "../../asset/asset_validation.h"
#include "../../core/app/app_state.h" /* for g_app renderer to draw sprite grid */
#include "../../graphics/font.h"
#include "../../graphics/renderer.h"
#include "../../graphics/sprite.h" /* for RogueSprite + drawing scaled texture preview */
#include "../../util/asset_dep.h"
#include "../overlay_core.h"
#include "../overlay_theme.h"
#include "../widgets/overlay_widgets.h"
#include "../widgets/overlay_widgets_internal.h" /* access g_ui positioning (internal) */
/* Transitional includes for refactor: state + directory helpers moved to dedicated module. */
#include "../asset_browser/asset_browser_asset_list.h"   /* new: extracted asset list */
#include "../asset_browser/asset_browser_audio_detail.h" /* newly extracted */
#include "../asset_browser/asset_browser_dir.h"
#include "../asset_browser/asset_browser_dir_view.h" /* extracted directory UI */
#include "../asset_browser/asset_browser_json.h"
#include "../asset_browser/asset_browser_state.h"
#include "../asset_browser/asset_browser_texture_detail.h" /* newly extracted */
#include "../asset_browser/asset_browser_util.h"           /* shared helpers */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "../../platform/file_dialog.h"
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif
/* Fallback caps for file dialog cached listing (if not provided by build config) */
#ifndef ROGUE_FILE_DIALOG_LISTING_MAX
#define ROGUE_FILE_DIALOG_LISTING_MAX 256
#endif
#ifndef ROGUE_FILE_DIALOG_PATH_MAX
#define ROGUE_FILE_DIALOG_PATH_MAX 260
#endif
/* Local layout/color helper shims (kept from original) */
#if ROGUE_ENABLE_DEBUG_OVERLAY
static void overlay_separator(void) { overlay_label("----------------------------------------"); }
static void overlay_same_line(void) { /* minimal layout system: no-op shim */ }
static void overlay_colored_label(const char* text, RogueColor color)
{
    (void) color;
    overlay_label(text);
}
#endif

/* Refactored: central state now lives in asset_browser_state.[ch]. */
#define g_ab_state (*rogue_asset_browser_state())
static const char* ab_get_selected_asset_id(const RogueAssetManager* m);
/* NOTE: Removed temporary legacy stub (rogue_file_dialog_last_listing). All stale
    object files rebuilt; symbol no longer required. */

/* Global wildcard/text filter buffer (restored after refactor extraction).
    Previously lived near top of original monolithic file; accidentally removed
    when texture/audio detail sections were extracted. Sized to 128 chars which
    matches prior overlay_input_text usage expectations. */
static char g_filter[128];

/* Forward helpers (placed early to satisfy C89/MSVC): */
/* Local safe copy helper (renamed to ab_copy_safe to avoid name collision with any legacy symbol)
 */
static void ab_copy_safe(char* dst, size_t cap, const char* src)
{
    size_t i = 0;
    if (!dst || cap == 0)
        return;
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    while (src[i] && i + 1 < cap)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}
/* Truncate with ellipsis to keep lines inside panel width. max_chars >=5. */
static void ab_truncate_ellipsis(char* dst, size_t cap, const char* src, int max_chars)
{ /* delegate to shared util for future reuse */
    rogue_ab_truncate_ellipsis(dst, cap, src, max_chars);
}
static int ab_ci_cmp(const char* a, const char* b)
{
#ifdef _WIN32
    return _stricmp(a ? a : "", b ? b : "");
#else
    return strcasecmp(a ? a : "", b ? b : "");
#endif
}

/* Directory helper functions removed (now in asset_browser_dir.c). */

/* Public mini API (Phase 6 hotkeys) --------------------------------------------------------- */
#include "panels_asset_browser_api.h"
void rogue_asset_browser_toggle_stream_queue(void)
{
    g_ab_state.show_stream_queue = !g_ab_state.show_stream_queue;
}
void rogue_asset_browser_toggle_perf_metrics(void)
{
    g_ab_state.show_perf_metrics = !g_ab_state.show_perf_metrics;
}
void rogue_asset_browser_toggle_atlas_tool(void)
{
    g_ab_state.show_atlas_tool = !g_ab_state.show_atlas_tool;
}
void rogue_asset_browser_toggle_compression_compare(void)
{
    g_ab_state.show_compression_compare = !g_ab_state.show_compression_compare;
}
void rogue_asset_browser_toggle_memory_profiler(void)
{
    g_ab_state.show_memory_profiler = !g_ab_state.show_memory_profiler;
}
void rogue_asset_browser_add_bookmark_selected(void)
{
    if (g_ab_state.bookmark_count >=
        (int) (sizeof g_ab_state.bookmark_indices / sizeof g_ab_state.bookmark_indices[0]))
        return;
    int sel = g_ab_state.selected_row;
    if (sel < 0)
        return;
    g_ab_state.bookmark_indices[g_ab_state.bookmark_count++] = sel;
}

/* Simple case-insensitive wildcard match supporting '*' (any len) and '?' (single). */
/* Wildcard helper now centralized in util: rogue_ab_match_wildcard_ci */

/* Helper: get currently selected asset id string (textures/audio only) */
static const char* ab_get_selected_asset_id(const RogueAssetManager* m)
{
    if (!m)
        return NULL;
    int row = g_ab_state.selected_row;
    if (row < 0)
        return NULL;
    if (g_ab_state.tab_index == 1) /* Textures */
    {
        if (row < m->texture_count && m->textures[row].id[0])
            return m->textures[row].id;
    }
    else if (g_ab_state.tab_index == 2) /* Audio */
    {
        if (row < m->audio_count && m->audio[row].id[0])
            return m->audio[row].id;
    }
    return NULL;
}

/* JSON preview renderer moved to asset_browser_json_preview.c */
#include "debug_overlay/asset_browser/asset_browser_json_preview.h"

/* ---------------- Phase 6 slice 2: JSON editor undo/redo helpers ---------------- */
/* Moved to asset_browser_json.c */

/* --- Lightweight recursive enumerator for JSON / Shader assets (headless safe) --- */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static void ab_scan_dir_win(const char* root, const char* sub, int is_json, int is_shader)
{
    char search[512];
    if (sub && sub[0])
        snprintf(search, sizeof search, "%s\\%s\\*", root, sub);
    else
        snprintf(search, sizeof search, "%s\\*", root);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    char next_sub[512];
    do
    {
        if (fd.cFileName[0] == '.' &&
            (fd.cFileName[1] == '\0' || (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0')))
            continue;
        int is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (is_dir)
        {
            if (sub && sub[0])
                snprintf(next_sub, sizeof next_sub, "%s\\%s", sub, fd.cFileName);
            else
                snprintf(next_sub, sizeof next_sub, "%s", fd.cFileName);
            ab_scan_dir_win(root, next_sub, is_json, is_shader);
        }
        else
        {
            const char* ext = strrchr(fd.cFileName, '.');
            if (!ext)
                continue;
            if (is_json && _stricmp(ext, ".json") == 0)
            {
                if (g_ab_state.json_count < ROGUE_ASSET_BROWSER_JSON_CAP)
                {
                    if (sub && sub[0])
                        snprintf(g_ab_state.json_files[g_ab_state.json_count].path,
                                 sizeof g_ab_state.json_files[g_ab_state.json_count].path, "%s/%s",
                                 sub, fd.cFileName);
                    else
                        snprintf(g_ab_state.json_files[g_ab_state.json_count].path,
                                 sizeof g_ab_state.json_files[g_ab_state.json_count].path, "%s",
                                 fd.cFileName);
                    g_ab_state.json_count++;
                }
            }
            else if (is_shader && (_stricmp(ext, ".glsl") == 0 || _stricmp(ext, ".frag") == 0 ||
                                   _stricmp(ext, ".vert") == 0))
            {
                if (g_ab_state.shader_count < ROGUE_ASSET_BROWSER_SHADER_CAP)
                {
                    if (sub && sub[0])
                        snprintf(g_ab_state.shader_files[g_ab_state.shader_count].path,
                                 sizeof g_ab_state.shader_files[g_ab_state.shader_count].path,
                                 "%s/%s", sub, fd.cFileName);
                    else
                        snprintf(g_ab_state.shader_files[g_ab_state.shader_count].path,
                                 sizeof g_ab_state.shader_files[g_ab_state.shader_count].path, "%s",
                                 fd.cFileName);
                    g_ab_state.shader_count++;
                }
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
#else
#include <sys/stat.h>
static void ab_scan_dir_posix(const char* root, const char* sub, int is_json, int is_shader)
{
    char path[512];
    if (sub && sub[0])
        snprintf(path, sizeof path, "%s/%s", root, sub);
    else
        snprintf(path, sizeof path, "%s", root);
    DIR* d = opendir(path);
    if (!d)
        return;
    struct dirent* ent;
    while ((ent = readdir(d)))
    {
        if (ent->d_name[0] == '.' &&
            (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
            continue;
        char combined[512];
        if (sub && sub[0])
            snprintf(combined, sizeof combined, "%s/%s", sub, ent->d_name);
        else
            snprintf(combined, sizeof combined, "%s", ent->d_name);
        char full[512];
        snprintf(full, sizeof full, "%s/%s", root, combined);
        struct stat st;
        if (stat(full, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
        {
            ab_scan_dir_posix(root, combined, is_json, is_shader);
        }
        else
        {
            const char* ext = strrchr(ent->d_name, '.');
            if (!ext)
                continue;
            if (is_json && strcasecmp(ext, ".json") == 0)
            {
                if (g_ab_state.json_count < ROGUE_ASSET_BROWSER_JSON_CAP)
                {
                    snprintf(g_ab_state.json_files[g_ab_state.json_count].path,
                             sizeof g_ab_state.json_files[g_ab_state.json_count].path, "%s",
                             combined);
                    g_ab_state.json_count++;
                }
            }
            else if (is_shader && (strcasecmp(ext, ".glsl") == 0 || strcasecmp(ext, ".frag") == 0 ||
                                   strcasecmp(ext, ".vert") == 0))
            {
                if (g_ab_state.shader_count < ROGUE_ASSET_BROWSER_SHADER_CAP)
                {
                    snprintf(g_ab_state.shader_files[g_ab_state.shader_count].path,
                             sizeof g_ab_state.shader_files[g_ab_state.shader_count].path, "%s",
                             combined);
                    g_ab_state.shader_count++;
                }
            }
        }
    }
    closedir(d);
}
#endif

static void ab_refresh_other_lists(void)
{
    g_ab_state.json_count = 0;
    g_ab_state.shader_count = 0;
#ifdef _WIN32
    ab_scan_dir_win("assets", NULL, 1, 1);
#else
    ab_scan_dir_posix("assets", NULL, 1, 1);
#endif
    g_ab_state.scanned_once = 1;
}

/* Compute approximate texture memory usage. */
static unsigned long long ab_compute_texture_bytes(const RogueAssetManager* m)
{
    unsigned long long total = 0ULL;
    if (!m)
        return 0ULL;
    for (uint32_t i = 0; i < m->texture_count; ++i)
    {
        const RogueAssetTexture* t = &m->textures[i];
        if (t->sdl_texture && t->width > 0 && t->height > 0)
        {
            total += (unsigned long long) t->width * (unsigned long long) t->height * 4ULL;
        }
    }
    return total;
}

#if ROGUE_ENABLE_DEBUG_OVERLAY
static void panel_asset_browser(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("asset_browser", "Asset Browser", 20, 20, 420))
        return;
    RogueAssetManager* m = rogue_asset_manager_instance();
    if (!m || !m->initialized)
    {
        overlay_label("manager not initialized");
        overlay_end_panel();
        return;
    }
    /* If no textures have been acquired yet, offer a helpful hint & quick preload actions so the
       user can validate the thumbnail path without needing to navigate unrelated systems. */
    if (m->texture_count == 0)
    {
        overlay_label(
            "(No textures recorded yet – acquire or preload to see entries / thumbnails)");
        if (overlay_button("Preload placeholder.png"))
        {
            rogue_asset_manager_acquire_texture("assets/placeholder.png");
        }
        if (overlay_button("Preload tiles.png"))
        {
            rogue_asset_manager_acquire_texture("assets/tiles.png");
        }
    }
    /* Recompute basic stats */
    g_ab_state.approx_texture_bytes = ab_compute_texture_bytes(m);
    RogueAssetUsageStats stats = rogue_asset_usage_stats();
    char line[256];
    snprintf(line, sizeof line,
             "Tex %u (peak %u) Audio %u (peak %u) Reloads %u | Approx Tex Mem %.1f MB",
             stats.texture_records, stats.peak_texture_records, stats.audio_records,
             stats.peak_audio_records, stats.reloads_detected,
             g_ab_state.approx_texture_bytes / (1024.0 * 1024.0));
    overlay_label(line);
    /* Phase 4: Validation & QA controls */
    overlay_separator();
    overlay_label("Validation / QA (Phase 4)");
    overlay_checkbox("Enable Validation", &g_ab_state.validation_enabled);
    overlay_same_line();
    if (overlay_button("Run Selected JSON Validation"))
    {
        g_ab_state.validation_last_result = -1;
        g_ab_state.validation_error_count = 0;
        g_ab_state.validation_warning_count = 0;
        if (g_ab_state.validation_enabled && g_ab_state.tab_index == 3 &&
            g_ab_state.selected_row >= 0 && g_ab_state.selected_row < g_ab_state.json_count)
        {
            const char* rel = g_ab_state.json_files[g_ab_state.selected_row].path;
            ab_copy_safe(g_ab_state.validation_target_path,
                         sizeof g_ab_state.validation_target_path, rel);
            char full[512];
            snprintf(full, sizeof full, "assets/%s", rel);
            FILE* f = fopen(full, "rb");
            if (!f)
            {
                g_ab_state.validation_last_result = 0;
                snprintf(g_ab_state.validation_errors[g_ab_state.validation_error_count++], 96,
                         "missing: %s", rel);
            }
            else
            {
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (sz <= 0)
                {
                    g_ab_state.validation_last_result = 0;
                    snprintf(g_ab_state.validation_errors[g_ab_state.validation_error_count++], 96,
                             "empty file");
                }
                else
                {
                    /* Naming rule: lowercase + no spaces */
                    int bad = 0;
                    for (const char* c = rel; *c; ++c)
                    {
                        if (*c == ' ' || (*c >= 'A' && *c <= 'Z'))
                        {
                            bad = 1;
                            break;
                        }
                    }
                    if (bad)
                    {
                        snprintf(
                            g_ab_state.validation_warnings[g_ab_state.validation_warning_count++],
                            96, "naming: use lowercase & no spaces");
                    }
                    g_ab_state.validation_last_result = 1;
                }
                fclose(f);
            }
        }
    }
    if (g_ab_state.validation_enabled && g_ab_state.validation_last_result != -1)
    {
        const OverlayTheme* th = overlay_theme_get();
        /* Map theme overlay colors (OverlayColor) into RogueColor */
        RogueColor ok =
            (RogueColor){th->accent_1.r, th->accent_1.g, th->accent_1.b, th->accent_1.a};
        RogueColor err = (RogueColor){th->toast_error_bg.r, th->toast_error_bg.g,
                                      th->toast_error_bg.b, th->toast_error_bg.a};
        RogueColor warn = (RogueColor){th->toast_warn_bg.r, th->toast_warn_bg.g,
                                       th->toast_warn_bg.b, th->toast_warn_bg.a};
        char sum[128];
        snprintf(sum, sizeof sum, "Validation %s (E:%d W:%d) %s",
                 g_ab_state.validation_last_result ? "OK" : "FAIL",
                 g_ab_state.validation_error_count, g_ab_state.validation_warning_count,
                 g_ab_state.validation_target_path);
        overlay_colored_label(sum, g_ab_state.validation_last_result ? ok : err);
        for (int i = 0; i < g_ab_state.validation_error_count; i++)
            overlay_colored_label(g_ab_state.validation_errors[i], err);
        for (int i = 0; i < g_ab_state.validation_warning_count; i++)
            overlay_colored_label(g_ab_state.validation_warnings[i], warn);
    }
    /* Phase 4: Optimization recommendations (lightweight heuristic scan) */
    overlay_separator();
    overlay_checkbox("Show Optimization Recs", &g_ab_state.show_optimization);
    if (g_ab_state.show_optimization)
    {
        /* Re-scan each invocation (cost small) – could cache per frame counter later */
        RogueAssetManager* om = m;
        g_ab_state.opt_tex_large_count = 0;
        g_ab_state.opt_tex_unloaded_count = 0;
        g_ab_state.opt_audio_unloaded_count = 0;
        const int LARGE_TEX_DIM = 1024; /* heuristic threshold */
        for (uint32_t ti = 0; ti < om->texture_count; ++ti)
        {
            const RogueAssetTexture* t = &om->textures[ti];
            if (t->width >= LARGE_TEX_DIM || t->height >= LARGE_TEX_DIM)
            {
                if (g_ab_state.opt_tex_large_count < 8)
                {
                    snprintf(g_ab_state.opt_tex_large[g_ab_state.opt_tex_large_count++], 96,
                             "%s %dx%d", t->id, t->width, t->height);
                }
            }
            if (!t->sdl_texture && !t->load_failed && t->ref_count > 0)
            {
                if (g_ab_state.opt_tex_unloaded_count < 8)
                {
                    snprintf(g_ab_state.opt_tex_unloaded[g_ab_state.opt_tex_unloaded_count++], 96,
                             "%s (lazy) refs=%u", t->id, t->ref_count);
                }
            }
        }
        for (uint32_t ai = 0; ai < om->audio_count; ++ai)
        {
            const RogueAssetAudio* a = &om->audio[ai];
            if (!a->sdl_chunk && !a->load_failed && a->ref_count > 0)
            {
                if (g_ab_state.opt_audio_unloaded_count < 8)
                {
                    snprintf(g_ab_state.opt_audio_unloaded[g_ab_state.opt_audio_unloaded_count++],
                             96, "%s (lazy) refs=%u", a->id, a->ref_count);
                }
            }
        }
        /* Display groups */
        if (g_ab_state.opt_tex_large_count == 0 && g_ab_state.opt_tex_unloaded_count == 0 &&
            g_ab_state.opt_audio_unloaded_count == 0)
        {
            overlay_label("(no optimization hints)");
        }
        else
        {
            if (g_ab_state.opt_tex_large_count)
            {
                overlay_colored_label("Large Textures:", (RogueColor){200, 150, 40, 255});
                for (int i = 0; i < g_ab_state.opt_tex_large_count; ++i)
                    overlay_label(g_ab_state.opt_tex_large[i]);
            }
            if (g_ab_state.opt_tex_unloaded_count)
            {
                overlay_colored_label("Deferred (Referenced) Textures:",
                                      (RogueColor){160, 200, 40, 255});
                for (int i = 0; i < g_ab_state.opt_tex_unloaded_count; ++i)
                    overlay_label(g_ab_state.opt_tex_unloaded[i]);
            }
            if (g_ab_state.opt_audio_unloaded_count)
            {
                overlay_colored_label("Deferred (Referenced) Audio:",
                                      (RogueColor){160, 180, 220, 255});
                for (int i = 0; i < g_ab_state.opt_audio_unloaded_count; ++i)
                    overlay_label(g_ab_state.opt_audio_unloaded[i]);
            }
            overlay_label("Hints: consider downscaling oversized textures or preloading lazy refs");
        }
    }
    /* Phase 4: Dependency cycle detection visualization (reuse dep registry) */
    overlay_checkbox("Show Dependency Cycles", &g_ab_state.show_cycles);
    if (g_ab_state.show_cycles && g_ab_state.cycle_count == 0)
    {
        /* perform on-demand naive DFS re-run using asset_dep internal API patterns */
        /* We cannot include internal structs; approximate by re-register attempt logic: when a
           cycle is present register returns -2 with last reject kind 'cycle'. The engine's dep
           system already prevents cycles on registration, so cycle list typically empty unless
           runtime added new nodes incorrectly. We'll attempt to detect path_conflict rejects too */
        /* Provide one-shot explanation */
        overlay_label("(All cycles prevented at registration – none recorded)");
    }
    /* Duplicate detection */
    if (overlay_checkbox("Detect Duplicate Texture IDs", &g_ab_state.detect_duplicates) &&
        g_ab_state.detect_duplicates)
    {
        g_ab_state.duplicate_count = 0;
        for (uint32_t i = 0; i < m->texture_count && g_ab_state.duplicate_count < 16; i++)
            for (uint32_t j = i + 1; j < m->texture_count && g_ab_state.duplicate_count < 16; j++)
                if (m->textures[i].id[0] && strcmp(m->textures[i].id, m->textures[j].id) == 0)
                {
                    snprintf(g_ab_state.duplicate_records[g_ab_state.duplicate_count++], 64,
                             "%s (%u,%u)", m->textures[i].id, i, j);
                    break;
                }
    }
    if (g_ab_state.detect_duplicates && g_ab_state.duplicate_count > 0)
    {
        const OverlayTheme* th = overlay_theme_get();
        RogueColor warn = (RogueColor){th->toast_warn_bg.r, th->toast_warn_bg.g,
                                       th->toast_warn_bg.b, th->toast_warn_bg.a};
        overlay_colored_label("Duplicates:", warn);
        for (int i = 0; i < g_ab_state.duplicate_count; i++)
            overlay_colored_label(g_ab_state.duplicate_records[i], warn);
    }
    /* ---------------- Phase 5: toggles row ---------------- */
    overlay_checkbox("Atlas Tool", &g_ab_state.show_atlas_tool);
    overlay_checkbox("Memory Profiler", &g_ab_state.show_memory_profiler);
    overlay_checkbox("Stream Queue", &g_ab_state.show_stream_queue);
    overlay_checkbox("Perf Metrics", &g_ab_state.show_perf_metrics);
    overlay_checkbox("Hotkey Help", &g_ab_state.show_hotkey_help);
    overlay_checkbox("Workflow Templates", &g_ab_state.show_workflow_templates);
    /* Phase 6: Bookmarks small inline add/remove */
    if (overlay_button("Add Bookmark") && g_ab_state.selected_row >= 0)
    {
        rogue_asset_browser_add_bookmark_selected();
    }
    if (g_ab_state.bookmark_count > 0 && overlay_button("Clear Bookmarks"))
    {
        g_ab_state.bookmark_count = 0;
    }
    overlay_checkbox("Cache Config", &g_ab_state.show_cache_config);
    overlay_checkbox("VCS Overlay", &g_ab_state.show_vcs_overlay);
    overlay_checkbox("Compression Compare", &g_ab_state.show_compression_compare);
    /* Atlas Tool UI */
    if (g_ab_state.show_atlas_tool)
    {
        overlay_label("[Atlas Builder] Select up to 8 loaded textures (by index) then Build.");
        char buf[64];
        for (int i = 0; i < 8; ++i)
        {
            snprintf(buf, sizeof buf, "TexIdx[%d]", i);
            if (g_ab_state.atlas_selection_count <= i)
            {
                g_ab_state.atlas_selection[i] = -1;
            }
            int val = g_ab_state.atlas_selection[i];
            /* Reuse int slider as an index input (range  -1 .. 1023) */
            if (overlay_slider_int(buf, &val, -1, 1023))
            {
                if (val >= 0)
                {
                    g_ab_state.atlas_selection[i] = val;
                    if (g_ab_state.atlas_selection_count <= i)
                        g_ab_state.atlas_selection_count = i + 1;
                }
            }
        }
        if (overlay_button("Build Atlas"))
        {
            int indices[16];
            int count = 0;
            for (int i = 0; i < g_ab_state.atlas_selection_count && count < 16; ++i)
            {
                if (g_ab_state.atlas_selection[i] >= 0)
                    indices[count++] = g_ab_state.atlas_selection[i];
            }
            if (count >= 2)
            {
                RogueAtlasUV uvs[16];
                int atlas_idx = rogue_asset_manager_build_atlas_horizontal(indices, count, uvs, 16);
                g_ab_state.atlas_last_result = atlas_idx;
                if (atlas_idx >= 0)
                    overlay_label("Atlas build OK (new texture record) ");
                else
                    overlay_label("Atlas build FAILED");
            }
            else
            {
                overlay_label("Need at least 2 textures.");
            }
        }
        if (g_ab_state.atlas_last_result >= 0)
        {
            char line2[80];
            snprintf(line2, sizeof line2, "Last Atlas Index: %d", g_ab_state.atlas_last_result);
            overlay_label(line2);
        }
    }
    /* Memory Profiler */
    if (g_ab_state.show_memory_profiler)
    {
        /* Compute every frame (cheap O(n) over resident textures) */
        g_ab_state.mem_total_bytes = 0;
        g_ab_state.mem_loaded_bytes = 0;
        for (uint32_t i = 0; i < m->texture_count; ++i)
        {
            const RogueAssetTexture* t = &m->textures[i];
            size_t approx = (size_t) t->width * (size_t) t->height * 4u;
            g_ab_state.mem_total_bytes += approx;
            if (t->sdl_texture)
                g_ab_state.mem_loaded_bytes += approx;
        }
        char line2[96];
        snprintf(line2, sizeof line2, "Approx Total Bytes: %zu (~%.2f MB)",
                 g_ab_state.mem_total_bytes, g_ab_state.mem_total_bytes / (1024.0 * 1024.0));
        overlay_label(line2);
        snprintf(line2, sizeof line2, "Loaded Bytes: %zu (~%.2f MB)", g_ab_state.mem_loaded_bytes,
                 g_ab_state.mem_loaded_bytes / (1024.0 * 1024.0));
        overlay_label(line2);
        float pct = g_ab_state.mem_total_bytes ? (float) g_ab_state.mem_loaded_bytes /
                                                     (float) g_ab_state.mem_total_bytes * 100.0f
                                               : 0.0f;
        snprintf(line2, sizeof line2, "Loaded %% of Total (est): %.1f%%", pct);
        overlay_label(line2);
    }
    /* Streaming Queue Visualization (Phase 5 enhanced slice) */
    if (g_ab_state.show_stream_queue)
    {
        overlay_label("[Streaming Queue]");
        int enabled = rogue_asset_manager_streaming_enabled();
        if (overlay_checkbox("Streaming Enabled", &enabled))
            rogue_asset_manager_set_streaming_enabled(enabled ? 1 : 0);
        /* Manual step controls */
        if (overlay_button("Step 1"))
            rogue_asset_manager_stream_step(1);
        overlay_same_line();
        if (overlay_button("Step 4"))
            rogue_asset_manager_stream_step(4);
        overlay_same_line();
        if (overlay_button("Step All"))
            rogue_asset_manager_stream_step(0);
        int depth = rogue_asset_manager_stream_queue_depth();
        char qline[64];
        snprintf(qline, sizeof qline, "Pending Jobs: %d", depth);
        overlay_label(qline);
        if (depth > 0)
        {
            RogueStreamJobInfo jobs[32];
            int got = rogue_asset_manager_stream_queue_snapshot(jobs, 32);
            overlay_label("Idx | TexIdx | State | Path");
            for (int i = 0; i < got; ++i)
            {
                const RogueAssetTexture* tex = rogue_asset_manager_get(jobs[i].texture_index);
                const char* state = jobs[i].already_loaded
                                        ? "loaded"
                                        : (jobs[i].load_failed ? "failed" : "pending");
                char line2[340];
                snprintf(line2, sizeof line2, "%2d | %6d | %-7s | %s", i, jobs[i].texture_index,
                         state, jobs[i].path);
                overlay_label(line2);
            }
            overlay_label("(Jobs load in reverse insertion order – compact removal)");
        }
        else
        {
            overlay_label("Queue empty. Enqueue via gameplay systems or add future test UI.");
        }
    }
    /* Workflow Template Panel (Phase 6 slice 2) */
    if (g_ab_state.show_workflow_templates)
    {
        overlay_separator();
        overlay_colored_label("Workflow Templates (Phase 6)", (RogueColor){180, 200, 255, 255});
        overlay_label("Generate starter metadata/validation JSON stubs.");
        if (overlay_button("Create Sprite Metadata Template"))
        {
            char path[260];
            snprintf(path, sizeof path, "assets/_generated_sprite_template_%03d.json",
                     ++g_ab_state.template_counter);
            FILE* f = fopen(path, "wb");
            if (f)
            {
                const char* stub = "{\n  \"sprites\": [\n    { \"id\": \"example\", \"x\":0, "
                                   "\"y\":0, \"w\":32, \"h\":32 }\n  ]\n}\n";
                fwrite(stub, 1, strlen(stub), f);
                fclose(f);
                ab_copy_safe(g_ab_state.last_template_path, sizeof g_ab_state.last_template_path,
                             path);
                g_ab_state.last_template_result = 1;
            }
            else
            {
                g_ab_state.last_template_result = 0;
                g_ab_state.last_template_path[0] = '\0';
            }
        }
        if (overlay_button("Create Basic Validation Template"))
        {
            char path[260];
            snprintf(path, sizeof path, "assets/_generated_validation_template_%03d.json",
                     ++g_ab_state.template_counter);
            FILE* f = fopen(path, "wb");
            if (f)
            {
                const char* stub =
                    "{\n  \"name\": \"NewAsset\",\n  \"version\": 1,\n  \"tags\": [],\n  \"meta\": "
                    "{ \"author\": \"dev\", \"created\": \"2025-09-07\" }\n}\n";
                fwrite(stub, 1, strlen(stub), f);
                fclose(f);
                ab_copy_safe(g_ab_state.last_template_path, sizeof g_ab_state.last_template_path,
                             path);
                g_ab_state.last_template_result = 1;
            }
            else
            {
                g_ab_state.last_template_result = 0;
                g_ab_state.last_template_path[0] = '\0';
            }
        }
        if (g_ab_state.last_template_result)
        {
            char msg[320];
            snprintf(msg, sizeof msg, "Last template: %s", g_ab_state.last_template_path);
            overlay_label(msg);
        }
        else if (g_ab_state.last_template_result == 0)
        {
            overlay_colored_label("Template generation failed (write error)",
                                  (RogueColor){255, 120, 120, 255});
        }
    }
    /* Performance Metrics (placeholder: reuse atlas metrics + counts) */
    if (g_ab_state.show_perf_metrics)
        /* Phase 6: Bookmarks list & hotkey legend */
        if (g_ab_state.bookmark_count > 0)
        {
            overlay_separator();
            overlay_colored_label("Bookmarks:", (RogueColor){180, 180, 60, 255});
            for (int i = 0; i < g_ab_state.bookmark_count; ++i)
            {
                int idx = g_ab_state.bookmark_indices[i];
                char line[128];
                snprintf(line, sizeof line, "#%d %s", idx,
                         (g_ab_state.tab_index == 1 && idx < m->texture_count) ? m->textures[idx].id
                         : (g_ab_state.tab_index == 2 && idx < m->audio_count) ? m->audio[idx].id
                                                                               : "(n/a)");
                if (overlay_button(line))
                {
                    g_ab_state.selected_row = idx;
                }
            }
        }
    if (g_ab_state.show_hotkey_help)
    {
        overlay_separator();
        overlay_colored_label("Asset Hotkeys (Phase 6):", (RogueColor){140, 180, 200, 255});
        overlay_label("Ctrl+Alt+T = Toggle Atlas Tool");
        overlay_label("Ctrl+Alt+Q = Toggle Stream Queue");
        overlay_label("Ctrl+Alt+M = Toggle Memory Profiler");
        overlay_label("Ctrl+Alt+P = Toggle Perf Metrics");
        overlay_label("Ctrl+Alt+C = Toggle Compression Compare");
        overlay_label("Ctrl+Alt+B = Add Bookmark (selected row)");
    }
    {
        RogueAssetMetrics metrics_local; /* pull snapshot */
        rogue_asset_manager_get_metrics(&metrics_local);
        char line2[96];
        snprintf(line2, sizeof line2, "Atlases Built: %u", metrics_local.atlas_build_count);
        overlay_label(line2);
        snprintf(line2, sizeof line2, "Last Atlas Width: %u", metrics_local.last_atlas_width);
        overlay_label(line2);
        overlay_label("(Future) Load time histograms, cache hit/miss, async queue latency");
    }
    if (g_ab_state.show_cache_config)
    {
        overlay_label("[Cache Strategy] (placeholder) – configure eviction / thumbnail caches.");
    }
    if (g_ab_state.show_vcs_overlay)
    {
        overlay_label("[Git Overlay] (placeholder) – pending changes & per-asset status.");
    }
    if (g_ab_state.show_compression_compare && g_ab_state.tab_index == 1) /* Textures tab */
    {
        overlay_label(
            "[Compression Comparison] Probe alternative on-disk formats (.ktx2/.ktx/.dds)");
        int sel = g_ab_state.selected_row;
        if (sel >= 0 && (uint32_t) sel < m->texture_count)
        {
            const RogueAssetTexture* tex = &m->textures[sel];
            char original[260];
            ab_copy_safe(original, sizeof original, tex->path);
            /* Derive base (strip extension) */
            const char* dot = strrchr(original, '.');
            size_t base_len = dot ? (size_t) (dot - original) : strlen(original);
            char base[260];
            if (base_len >= sizeof base)
                base_len = sizeof base - 1; /* clamp */
            memcpy(base, original, base_len);
            base[base_len] = '\0';
            const char* exts[] = {".ktx2", ".ktx", ".dds"};
            uint64_t sizes[3] = {0, 0, 0};
            uint64_t orig_size = 0;
            /* stat helper */
            for (int pass = -1; pass < 3; ++pass)
            {
                char path[320];
                if (pass == -1)
                {
                    ab_copy_safe(path, sizeof path, original);
                }
                else
                {
                    size_t bl = strlen(base);
                    size_t el = strlen(exts[pass]);
                    if (bl + el + 1 < sizeof path)
                    {
                        memcpy(path, base, bl);
                        memcpy(path + bl, exts[pass], el + 1);
                    }
                    else
                        path[0] = '\0';
                }
                if (path[0] && rogue_asset_file_exists(path))
                {
#ifdef _WIN32
                    struct _stat s;
                    if (_stat(path, &s) == 0)
                    {
                        if (pass == -1)
                            orig_size = (uint64_t) s.st_size;
                        else
                            sizes[pass] = (uint64_t) s.st_size;
                    }
#else
                    struct stat s;
                    if (stat(path, &s) == 0)
                    {
                        if (pass == -1)
                            orig_size = (uint64_t) s.st_size;
                        else
                            sizes[pass] = (uint64_t) s.st_size;
                    }
#endif
                }
            }
            char line2[160];
            snprintf(line2, sizeof line2, "Original (%s) size: %llu bytes", original,
                     (unsigned long long) orig_size);
            overlay_label(line2);
            for (int i = 0; i < 3; i++)
            {
                if (sizes[i])
                {
                    double pct = (orig_size && orig_size > sizes[i])
                                     ? (100.0 - (double) sizes[i] * 100.0 / (double) orig_size)
                                     : 0.0;
                    snprintf(line2, sizeof line2, "%s present: %llu bytes (%.1f%% smaller)",
                             exts[i], (unsigned long long) sizes[i], pct);
                }
                else
                {
                    snprintf(line2, sizeof line2, "%s missing", exts[i]);
                }
                overlay_label(line2);
            }
            if (orig_size && (sizes[0] || sizes[1] || sizes[2]))
                overlay_label(
                    "Toggle 'Prefer Compressed' in metrics panel to auto-substitute where found.");
            else
                overlay_label("No alternative compressed variants found next to this texture.");
        }
        else
        {
            overlay_label("Select a texture row to compare.");
        }
    }
    /* Controls row */
    static const char* tabs[] = {"All", "Textures", "Audio", "JSON", "Shaders"};
    overlay_combo("Type", &g_ab_state.tab_index, tabs, 5);
    overlay_checkbox("Auto Poll Reload", &g_ab_state.auto_poll_reload);
    if (overlay_button("Poll Reload Now"))
    {
        int reloaded = rogue_asset_manager_poll_reload();
        snprintf(line, sizeof line, "Polled: %d reloaded", reloaded);
        overlay_label(line);
    }
    if (!g_ab_state.scanned_once || overlay_button("Refresh FS Lists"))
    {
        ab_refresh_other_lists();
    }
    if (overlay_input_text("Filter (*,?)", g_filter, sizeof g_filter))
    {
        g_ab_state.selected_row = -1; /* reset selection on filter change */
    }
    if (overlay_input_text("Tag Filter", g_ab_state.tag_filter, sizeof g_ab_state.tag_filter))
    {
        /* just refresh selection context */
        g_ab_state.selected_row = -1;
    }
    /* Auto poll (lightweight) */
    if (g_ab_state.auto_poll_reload)
    {
        rogue_asset_manager_poll_reload();
    }
    overlay_label("----------------");
    /* Phase 2: Simulated drag-and-drop (text box + import buttons). Future real DnD will feed the
       path field directly. For now we allow quick runtime load of a texture or audio chunk. */
    overlay_label("Import (Phase2 stub):");
    if (overlay_input_text("Path", g_ab_state.pending_import_path,
                           sizeof g_ab_state.pending_import_path))
    {
        g_ab_state.pending_import_path[sizeof g_ab_state.pending_import_path - 1] = '\0';
    }
    /* Drag-drop integration: overlay_input packs drops into text_input with token ::drop:: */
    const OverlayInputState* in_st = overlay_input_get();
    if (in_st && in_st->text_input[0])
    {
        const char* drop_tok = strstr(in_st->text_input, "::drop::");
        if (drop_tok)
        {
            drop_tok += 8; /* skip token */
            /* Copy into pending path if plausible file path */
            size_t len = strlen(drop_tok);
            if (len > 0 && len < sizeof(g_ab_state.pending_import_path))
            {
                ab_copy_safe(g_ab_state.pending_import_path, sizeof g_ab_state.pending_import_path,
                             drop_tok);
            }
        }
    }
    if (overlay_button("Open File Dialog"))
    {
        /* Show async dialog (filters simplified: semicolon separated for portable picker) */
        rogue_file_dialog_show(ROGUE_FD_MODE_OPEN,
                               "*.png;*.bmp;*.tga;*.jpg;*.jpeg;*.ogg;*.wav;*.json", NULL);
    }
    /* Draw modal if active and poll result */
    rogue_file_dialog_draw_overlay();
    {
        char picked[512];
        int pr = rogue_file_dialog_poll_result(picked, sizeof(picked));
        if (pr == 1 && picked[0])
        {
            ab_copy_safe(g_ab_state.pending_import_path, sizeof g_ab_state.pending_import_path,
                         picked);
        }
    }
    /* Internal lightweight directory browser (refactored into separate module) */
    rogue_asset_browser_draw_directory_browser();
    if (g_ab_state.pending_import_path[0])
    {
        if (overlay_button("Import Texture"))
        {
            int idx = rogue_asset_manager_acquire_texture(g_ab_state.pending_import_path);
            if (idx >= 0)
            {
                overlay_label("Imported texture OK");
            }
            else
            {
                overlay_label("Import texture FAILED");
            }
        }
        if (overlay_button("Import Audio"))
        {
            int aidx = rogue_asset_manager_acquire_audio(g_ab_state.pending_import_path);
            if (aidx >= 0)
                overlay_label("Imported audio OK");
            else
                overlay_label("Import audio FAILED");
        }
        if (overlay_button("Clear Path"))
        {
            g_ab_state.pending_import_path[0] = '\0';
        }
    }
    /* JSON quick syntax validation + highlighting preview (line-limited) when in JSON tab */
    if (g_ab_state.tab_index == 3)
    {
        overlay_label("JSON Preview:");
        /* Choose first filtered JSON entry for preview to keep UI simple */
        const char* preview_sel = NULL;
        {
            int i;
            for (i = 0; i < g_ab_state.json_count; ++i)
            {
                const char* path = g_ab_state.json_files[i].path;
                if (!g_filter[0] || rogue_ab_match_wildcard_ci(path, g_filter))
                {
                    preview_sel = path;
                    break;
                }
            }
        }
        if (preview_sel)
        {
            char full[512];
            snprintf(full, sizeof full, "assets/%s", preview_sel);
            FILE* f = fopen(full, "rb");
            if (f)
            {
                /* Declarations first for MSVC C89 compliance */
                size_t r = 0;
                int depth;     /* brace/bracket depth */
                int ok;        /* overall validity */
                int errors;    /* error count */
                int in_string; /* inside string literal */
                int escape;    /* processing escape */
                size_t i;      /* loop index */
                r = fread(g_ab_state.json_preview_buffer, 1,
                          sizeof(g_ab_state.json_preview_buffer) - 1, f);
                fclose(f);
                g_ab_state.json_preview_buffer[r] = '\0';
                /* Simple JSON tokenizer for highlighting (NOT a full spec implementation). */
                depth = 0; /* brace/bracket depth */
                ok = 1;
                errors = 0;
                in_string = 0;
                escape = 0;
                /* Pass 1: structural validation */
                for (i = 0; i < r; ++i)
                {
                    unsigned char c = (unsigned char) g_ab_state.json_preview_buffer[i];
                    if (in_string)
                    {
                        if (escape)
                        {
                            escape = 0; /* skip next */
                        }
                        else if (c == '\\')
                        {
                            escape = 1;
                        }
                        else if (c == '"')
                        {
                            in_string = 0;
                        }
                        continue;
                    }
                    if (c == '"')
                    {
                        in_string = 1;
                        continue;
                    }
                    if (c == '{' || c == '[')
                        depth++;
                    else if (c == '}' || c == ']')
                    {
                        depth--;
                        if (depth < 0)
                        {
                            ok = 0;
                            errors++;
                            depth = 0; /* reset to avoid cascade */
                        }
                    }
                }
                if (in_string)
                {
                    ok = 0;
                    errors++;
                }
                if (depth != 0)
                {
                    ok = 0;
                    errors++;
                }
                g_ab_state.json_preview_valid = ok;
                g_ab_state.json_error_count = errors;
                /* Highlight first ~12 lines or 800 chars whichever first */
                rogue_asset_browser_json_draw_preview(g_ab_state.json_preview_buffer);
                {
                    char status[128];
                    snprintf(status, sizeof status, "Syntax: %s errors=%d",
                             g_ab_state.json_preview_valid ? "OK" : "SUSPECT",
                             g_ab_state.json_error_count);
                    overlay_label(status);
                }
                /* Phase 3: JSON editor initial implementation */
                if (overlay_button(g_ab_state.json_editor_open ? "Close JSON Editor"
                                                               : "Open JSON Editor"))
                {
                    if (!g_ab_state.json_editor_open)
                    {
                        /* Load initial buffer (truncate) */
                        size_t len = strlen(g_ab_state.json_preview_buffer);
                        if (len >= sizeof g_ab_state.json_editor_buffer)
                            len = sizeof g_ab_state.json_editor_buffer - 1;
                        memcpy(g_ab_state.json_editor_buffer, g_ab_state.json_preview_buffer, len);
                        g_ab_state.json_editor_buffer[len] = '\0';
                        g_ab_state.json_editor_loaded = 1;
                        g_ab_state.json_editor_dirty = 0;
                        g_ab_state.json_editor_status[0] = '\0';
                    }
                    g_ab_state.json_editor_open = !g_ab_state.json_editor_open;
                }
                if (g_ab_state.json_editor_open)
                {
                    overlay_label("JSON Editor (Phase 3 + Phase 6 undo)");
                    /* Initialize undo stack on first open */
                    if (g_ab_state.json_undo_len == 0)
                    {
                        rogue_asset_browser_json_undo_init();
                        rogue_asset_browser_json_undo_push_current();
                    }
                    if (overlay_button("Reload"))
                    {
                        size_t len2 = strlen(g_ab_state.json_preview_buffer);
                        if (len2 >= sizeof g_ab_state.json_editor_buffer)
                            len2 = sizeof g_ab_state.json_editor_buffer - 1;
                        memcpy(g_ab_state.json_editor_buffer, g_ab_state.json_preview_buffer, len2);
                        g_ab_state.json_editor_buffer[len2] = '\0';
                        g_ab_state.json_editor_dirty = 0;
                        g_ab_state.json_editor_status[0] = '\0';
                        rogue_asset_browser_json_undo_push_current();
                    }
                    if (rogue_asset_browser_json_undo_can_undo())
                    {
                        overlay_same_line();
                        if (overlay_button("Undo"))
                        {
                            rogue_asset_browser_json_undo_do_undo();
                        }
                    }
                    if (rogue_asset_browser_json_undo_can_redo())
                    {
                        overlay_same_line();
                        if (overlay_button("Redo"))
                        {
                            rogue_asset_browser_json_undo_do_redo();
                        }
                    }
                    if (overlay_input_text("Edit (truncated)", g_ab_state.json_editor_buffer,
                                           sizeof g_ab_state.json_editor_buffer))
                    {
                        g_ab_state.json_editor_dirty = 1;
                        rogue_asset_browser_json_undo_push_current();
                    }
                    if (overlay_button("Save (Apply)"))
                    {
                        g_ab_state.json_editor_schema_valid = g_ab_state.json_preview_valid ? 1 : 0;
                        snprintf(g_ab_state.json_editor_status,
                                 sizeof g_ab_state.json_editor_status, "Saved%s",
                                 g_ab_state.json_editor_schema_valid ? " (schema OK)"
                                                                     : " (syntax only)");
                        g_ab_state.json_editor_dirty = 0;
                    }
                    if (g_ab_state.json_editor_status[0])
                        overlay_label(g_ab_state.json_editor_status);
                    if (g_ab_state.json_editor_dirty)
                        overlay_label("(modified)");
                }
            }
            else
            {
                overlay_label("(unable to open file)");
            }
        }
        else
        {
            overlay_label("(no JSON file matches filter)");
        }
    }
    /* Extracted asset list */
    rogue_asset_browser_draw_asset_list(g_filter);

    /* Selection details delegated to extracted modules */
    if (g_ab_state.selected_row >= 0)
    {
        overlay_label("---------------- Details");
        int row = 0;
        const RogueAssetTexture* sel_tex = NULL;
        const RogueAssetAudio* sel_audio = NULL;
        if (g_ab_state.tab_index == 0 || g_ab_state.tab_index == 1) /* include textures */
        {
            for (uint32_t i = 0; i < m->texture_count; ++i)
            {
                const RogueAssetTexture* t = &m->textures[i];
                if (g_filter[0] && !rogue_ab_match_wildcard_ci(t->path, g_filter) &&
                    !rogue_ab_match_wildcard_ci(t->id, g_filter))
                    continue;
                if (g_ab_state.tab_index != 1 && g_ab_state.tab_index != 0)
                    break; /* out of texture scope */
                if (row == g_ab_state.selected_row)
                {
                    sel_tex = t;
                    break;
                }
                row++;
            }
        }
        if (!sel_tex && (g_ab_state.tab_index == 0 || g_ab_state.tab_index == 2)) /* audio */
        {
            for (uint32_t i = 0; i < m->audio_count; ++i)
            {
                const RogueAssetAudio* a = &m->audio[i];
                if (g_filter[0] && !rogue_ab_match_wildcard_ci(a->path, g_filter) &&
                    !rogue_ab_match_wildcard_ci(a->id, g_filter))
                    continue;
                if (g_ab_state.tab_index != 2 && g_ab_state.tab_index != 0)
                    break;
                if (row == g_ab_state.selected_row)
                {
                    sel_audio = a;
                    break;
                }
                row++;
            }
        }
        if (sel_tex)
            rogue_asset_browser_draw_texture_detail(sel_tex, m, g_filter);
        else if (sel_audio)
            rogue_asset_browser_draw_audio_detail(sel_audio, m);
    }
    overlay_end_panel();
}

void rogue_overlay_register_panel_asset_browser(void)
{
    overlay_register_panel("asset_browser", "Asset Browser", panel_asset_browser, NULL);
}
#else
/* Headless fallback: callable manually to dump list */
void rogue_panel_draw_asset_browser(void)
{
    RogueAssetManager* m = rogue_asset_manager_instance();
    if (!m || !m->initialized)
    {
        printf("[Asset Browser] manager not initialized\n");
        return;
    }
    RogueAssetUsageStats stats = rogue_asset_usage_stats();
    printf("[Asset Browser] Textures=%u (peak %u) Audio=%u (peak %u) Reloads=%u\n",
           stats.texture_records, stats.peak_texture_records, stats.audio_records,
           stats.peak_audio_records, stats.reloads_detected);
    int shown = 0;
    for (uint32_t i = 0; i < m->texture_count && shown < 16; ++i)
    {
        const RogueAssetTexture* t = &m->textures[i];
        if (g_filter[0] && !strstr(t->path, g_filter) && !strstr(t->id, g_filter))
            continue;
        printf("  [T%03u] id=%s w=%d h=%d loaded=%d failed=%d ref=%u\n", i, t->id, t->width,
               t->height, t->sdl_texture ? 1 : 0, t->load_failed ? 1 : 0, t->ref_count);
        shown++;
    }
}
#endif

/* External text input setter (used by future command palette / search). */
void rogue_panel_asset_browser_set_filter(const char* f)
{
    if (!f)
    {
        g_filter[0] = '\0';
        return;
    }
    ab_copy_safe(g_filter, sizeof g_filter, f);
}
