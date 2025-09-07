/* panels_asset_browser.c - Phase 7: Visual Asset Browser
   Integrates with overlay when enabled; falls back to printf listing in
   headless builds. Provides substring filter + shows usage peaks & reloads.
*/
#include "../../asset/asset_manager.h"
#include "../../asset/asset_validation.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include <stdio.h>
#include <string.h>

static char g_filter[48];

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
    RogueAssetUsageStats stats = rogue_asset_usage_stats();
    char line[160];
    snprintf(line, sizeof line, "Tex %u (peak %u) Audio %u (peak %u) Reloads %u",
             stats.texture_records, stats.peak_texture_records, stats.audio_records,
             stats.peak_audio_records, stats.reloads_detected);
    overlay_label(line);
    if (overlay_input_text("Filter", g_filter, sizeof g_filter))
    {
        /* updated live */
    }
    overlay_label("----------------");
    int limit = 200; /* virtualized simple cap */
    for (uint32_t i = 0; i < m->texture_count && limit > 0; ++i)
    {
        const RogueAssetTexture* t = &m->textures[i];
        if (g_filter[0] && !strstr(t->path, g_filter) && !strstr(t->id, g_filter))
            continue;
        snprintf(line, sizeof line, "T%03u %s w=%d h=%d ref=%u%s%s", i, t->id, t->width, t->height,
                 t->ref_count, t->load_failed ? " FAIL" : "", t->sdl_texture ? " *" : "");
        overlay_label(line);
        limit--;
    }
    for (uint32_t i = 0; i < m->audio_count && limit > 0; ++i)
    {
        const RogueAssetAudio* a = &m->audio[i];
        if (g_filter[0] && !strstr(a->path, g_filter) && !strstr(a->id, g_filter))
            continue;
        snprintf(line, sizeof line, "A%03u %s ref=%u%s%s", i, a->id, a->ref_count,
                 a->load_failed ? " FAIL" : "", a->sdl_chunk ? " *" : "");
        overlay_label(line);
        limit--;
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
