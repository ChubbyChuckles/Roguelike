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
#include "../../graphics/sprite.h" /* for RogueSprite + drawing scaled texture preview */
#include "../../util/asset_dep.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "../widgets/overlay_widgets_internal.h" /* access g_ui positioning (internal) */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifndef ROGUE_ASSET_BROWSER_JSON_CAP
#define ROGUE_ASSET_BROWSER_JSON_CAP 512
#endif
#ifndef ROGUE_ASSET_BROWSER_SHADER_CAP
#define ROGUE_ASSET_BROWSER_SHADER_CAP 128
#endif

typedef struct AssetOtherRecord
{
    char path[260];
} AssetOtherRecord;

typedef struct AssetBrowserEnhancedState
{
    int tab_index;        /* 0=All,1=Textures,2=Audio,3=JSON,4=Shaders */
    int selected_row;     /* selection within current filtered view (textures/audio only) */
    int auto_poll_reload; /* bool */
    unsigned long long last_reload_poll_frame; /* monotonic counter (panel local) */
    /* Cached JSON & shader file lists */
    AssetOtherRecord json_files[ROGUE_ASSET_BROWSER_JSON_CAP];
    int json_count;
    AssetOtherRecord shader_files[ROGUE_ASSET_BROWSER_SHADER_CAP];
    int shader_count;
    int scanned_once; /* bool */
    /* Stats cache */
    unsigned long long approx_texture_bytes;
} AssetBrowserEnhancedState;

static AssetBrowserEnhancedState g_ab_state; /* zero-init */

static char g_filter[48];
static int g_filter_use_wildcards = 1; /* future: toggle real regex */

/* Simple case-insensitive wildcard match supporting '*' (any len) and '?' (single). */
static int ab_match_wildcard_ci(const char* text, const char* pattern)
{
    if (!pattern || !pattern[0])
        return 1; /* empty pattern matches all */
    if (!text)
        return 0;
    /* Iterative backtracking (non-recursive) */
    const char *t = text, *p = pattern;
    const char *star = NULL, *star_text = NULL;
    while (*t)
    {
        char pc = *p;
        if (pc == '*')
        {
            star = p++;
            star_text = t;
            continue;
        }
        if (pc == '?' || (pc && tolower((unsigned char) pc) == tolower((unsigned char) *t)))
        {
            p++;
            t++;
            continue;
        }
        if (star)
        {
            p = star + 1;
            t = ++star_text;
            continue;
        }
        return 0;
    }
    while (*p == '*')
        p++;
    return *p == '\0';
}

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
#include <dirent.h>
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
    /* Auto poll (lightweight) */
    if (g_ab_state.auto_poll_reload)
    {
        rogue_asset_manager_poll_reload();
    }
    overlay_label("----------------");
    int shown = 0;
    int limit = 300; /* soft cap */
    int tab = g_ab_state.tab_index;
    int current_row_index = 0; /* for selection mapping */
/* Helper lambda (emulated) to test filter */
#define PASS_FILTER(txt, id)                                                                       \
    (!g_filter[0] || ab_match_wildcard_ci((txt), (g_filter)) ||                                    \
     ab_match_wildcard_ci((id), (g_filter)))
    if (tab == 0 || tab == 1 || tab == 0)
    {
        for (uint32_t i = 0; i < m->texture_count && limit > 0; ++i)
        {
            const RogueAssetTexture* t = &m->textures[i];
            if (!PASS_FILTER(t->path, t->id))
                continue;
            if (tab == 2 || tab == 3 || tab == 4) /* texture not in audio/json/shader tabs */
                break; /* skip textures when audio/json/shader specific; rely on else blocks */
            snprintf(line, sizeof line, "T%03u %s w=%d h=%d ref=%u%s%s", i, t->id, t->width,
                     t->height, t->ref_count, t->load_failed ? " FAIL" : "",
                     t->sdl_texture ? " *" : "");
            if (g_ab_state.selected_row == current_row_index)
            {
                overlay_label(line);
            }
            else if (overlay_button(line))
            {
                g_ab_state.selected_row = current_row_index;
            }
            current_row_index++;
            shown++;
            limit--;
        }
    }
    if (tab == 0 || tab == 2)
    {
        for (uint32_t i = 0; i < m->audio_count && limit > 0; ++i)
        {
            const RogueAssetAudio* a = &m->audio[i];
            if (!PASS_FILTER(a->path, a->id))
                continue;
            if (tab == 1 || tab == 3 || tab == 4)
                break; /* only textures desired in those specific tabs */
            snprintf(line, sizeof line, "A%03u %s ref=%u%s%s", i, a->id, a->ref_count,
                     a->load_failed ? " FAIL" : "", a->sdl_chunk ? " *" : "");
            if (g_ab_state.selected_row == current_row_index)
                overlay_label(line);
            else if (overlay_button(line))
                g_ab_state.selected_row = current_row_index;
            current_row_index++;
            shown++;
            limit--;
        }
    }
    if (tab == 0 || tab == 3)
    {
        for (int i = 0; i < g_ab_state.json_count && limit > 0; ++i)
        {
            const char* path = g_ab_state.json_files[i].path;
            if (!PASS_FILTER(path, path))
                continue;
            snprintf(line, sizeof line, "J %s", path);
            overlay_label(line);
            shown++;
            limit--;
        }
    }
    if (tab == 0 || tab == 4)
    {
        for (int i = 0; i < g_ab_state.shader_count && limit > 0; ++i)
        {
            const char* path = g_ab_state.shader_files[i].path;
            if (!PASS_FILTER(path, path))
                continue;
            snprintf(line, sizeof line, "S %s", path);
            overlay_label(line);
            shown++;
            limit--;
        }
    }
#undef PASS_FILTER

    /* Selection details + dependency list (textures/audio only currently) */
    if (g_ab_state.selected_row >= 0)
    {
        overlay_label("---------------- Details");
        int row = 0;
        const RogueAssetTexture* sel_tex = NULL;
        const RogueAssetAudio* sel_audio = NULL;
        for (uint32_t i = 0; i < m->texture_count; ++i)
        {
            const RogueAssetTexture* t = &m->textures[i];
            if (!ab_match_wildcard_ci(t->path, g_filter) &&
                !ab_match_wildcard_ci(t->id, g_filter) && g_filter[0])
                continue;
            if (g_ab_state.tab_index == 2 || g_ab_state.tab_index == 3 || g_ab_state.tab_index == 4)
                break; /* not in texture scope */
            if (row == g_ab_state.selected_row)
            {
                sel_tex = t;
                break;
            }
            row++;
        }
        if (!sel_tex && (g_ab_state.tab_index == 0 || g_ab_state.tab_index == 2))
        {
            /* adjust row for textures count covered */
            for (uint32_t i = 0; i < m->audio_count; ++i)
            {
                const RogueAssetAudio* a = &m->audio[i];
                if (!ab_match_wildcard_ci(a->path, g_filter) &&
                    !ab_match_wildcard_ci(a->id, g_filter) && g_filter[0])
                    continue;
                if (g_ab_state.tab_index == 1 || g_ab_state.tab_index == 3 ||
                    g_ab_state.tab_index == 4)
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
        {
            snprintf(line, sizeof line,
                     "Selected Texture: id=%s w=%d h=%d ref=%u fail=%d loaded=%d", sel_tex->id,
                     sel_tex->width, sel_tex->height, sel_tex->ref_count,
                     sel_tex->load_failed ? 1 : 0, sel_tex->sdl_texture ? 1 : 0);
            overlay_label(line);
            /* Provide an on-demand ensure-load button if the record exists but the SDL texture
               isn't created yet (e.g., lazy mode future phases) */
#if defined(ROGUE_HAVE_SDL)
            if (!sel_tex->sdl_texture && !sel_tex->load_failed)
            {
                if (overlay_button("Force Load Now"))
                {
                    int idx = rogue_asset_manager_find_by_id(sel_tex->id);
                    if (idx >= 0)
                        rogue_asset_manager_ensure_texture_loaded(idx);
                }
            }
#endif
            /* Dependencies */
            const char* dep_ids[32];
            int depc = rogue_asset_dep_get_deps(sel_tex->id, dep_ids, 32);
            if (depc > 0)
            {
                overlay_label("Deps:");
                for (int di = 0; di < depc; ++di)
                {
                    overlay_label(dep_ids[di]);
                }
            }
            /* Basic inline thumbnail preview (Phase 1 thumbnail generation baseline). We simply
               draw the existing SDL texture scaled to fit a 96px width (no cache yet – the
               roadmap's Phase 7 memory-efficient cache will replace/extend this). */
#if defined(ROGUE_HAVE_SDL)
            if (sel_tex->sdl_texture && sel_tex->width > 0 && sel_tex->height > 0)
            {
                int max_w = 96;
                int scale = 1;
                if (sel_tex->width > 0)
                {
                    scale = max_w / sel_tex->width;
                    if (scale < 1)
                        scale = 1; /* at least 1x */
                    if (scale > 8)
                        scale = 8; /* clamp runaway tiny assets */
                }
                RogueTexture wrap = {0};
                wrap.handle = (SDL_Texture*) sel_tex->sdl_texture; /* non-owning */
                wrap.w = sel_tex->width;
                wrap.h = sel_tex->height;
                RogueSprite spr = {0};
                spr.tex = &wrap;
                spr.sw = wrap.w;
                spr.sh = wrap.h;
                /* Reserve a little vertical space separation */
                overlay_label("Preview:");
                int px = g_ui.cur_x;
                int py = g_ui.cur_y + 2;
                rogue_sprite_draw(&spr, px, py, scale);
                g_ui.cur_y = py + spr.sh * scale + 4; /* advance cursor */
            }
#endif
        }
        else if (sel_audio)
        {
            snprintf(line, sizeof line, "Selected Audio: id=%s ref=%u fail=%d loaded=%d",
                     sel_audio->id, sel_audio->ref_count, sel_audio->load_failed ? 1 : 0,
                     sel_audio->sdl_chunk ? 1 : 0);
            overlay_label(line);
            const char* dep_ids[32];
            int depc = rogue_asset_dep_get_deps(sel_audio->id, dep_ids, 32);
            if (depc > 0)
            {
                overlay_label("Deps:");
                for (int di = 0; di < depc; ++di)
                    overlay_label(dep_ids[di]);
            }
        }
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
#if defined(_MSC_VER)
    strncpy_s(g_filter, sizeof g_filter, f, _TRUNCATE);
#else
    strncpy(g_filter, f, sizeof g_filter - 1);
    g_filter[sizeof g_filter - 1] = '\0';
#endif
}
