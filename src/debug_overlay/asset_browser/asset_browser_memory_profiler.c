/* asset_browser_memory_profiler.c
   Extracted memory profiler UI from panels_asset_browser.c */

#include "debug_overlay/asset_browser/asset_browser_memory_profiler.h"
#include "../widgets/overlay_widgets.h"
#include "asset/asset_manager.h"
#include "debug_overlay/asset_browser/asset_browser_state.h"

#define g_ab_state (*rogue_asset_browser_state())

void rogue_asset_browser_draw_memory_profiler(RogueAssetManager* m)
{
    if (!m)
        return;
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
    snprintf(line2, sizeof line2, "Approx Total Bytes: %zu (~%.2f MB)", g_ab_state.mem_total_bytes,
             g_ab_state.mem_total_bytes / (1024.0 * 1024.0));
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
