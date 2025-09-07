/* panels_asset_browser.c - Phase 7: Visual Asset Browser (stub)
   Displays summary of loaded textures/audio with basic filtering.
   Safe in headless builds (no direct SDL texture operations).
*/
#include "../../asset/asset_manager.h"
#include "../../asset/asset_validation.h"
#include <stdio.h>
#include <string.h>

/* Overlay integration expects a registration macro elsewhere; we only provide
   a draw function symbol that the panel system will look up if compiled. */

static char g_filter[48];

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
    if (g_filter[0])
        printf("Filter: '%s'\n", g_filter);
    /* List a truncated table (first 16 entries each) honoring filter substring */
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
    shown = 0;
    for (uint32_t i = 0; i < m->audio_count && shown < 16; ++i)
    {
        const RogueAssetAudio* a = &m->audio[i];
        if (g_filter[0] && !strstr(a->path, g_filter) && !strstr(a->id, g_filter))
            continue;
        printf("  [A%03u] id=%s loaded=%d failed=%d ref=%u\n", i, a->id, a->sdl_chunk ? 1 : 0,
               a->load_failed ? 1 : 0, a->ref_count);
        shown++;
    }
}

/* Simple API for overlay text input binding (to be wired later). */
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
