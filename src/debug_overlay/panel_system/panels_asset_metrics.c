/* panels_asset_metrics.c - Asset Manager metrics & streaming queue debug panel */
#include "../../asset/asset_manager.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_asset_metrics(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("asset_metrics", "Asset Metrics", 340, 10, 320))
        return;
    RogueAssetMetrics m;
    rogue_asset_manager_get_metrics(&m);
    char line[128];
    snprintf(line, sizeof line, "Texture loads: %u (%.2f ms total)", m.texture_load_count,
             (double) m.texture_load_us / 1000.0);
    overlay_label(line);
    snprintf(line, sizeof line, "Audio loads: %u (%.2f ms total)", m.audio_load_count,
             (double) m.audio_load_us / 1000.0);
    overlay_label(line);
    snprintf(line, sizeof line, "Stream queue depth: %u", m.stream_queue_depth);
    overlay_label(line);
    snprintf(line, sizeof line, "Stream loaded: %u", m.stream_loaded_count);
    overlay_label(line);
    if (m.atlas_build_count > 0)
    {
        snprintf(line, sizeof line, "Atlases built: %u (last width %u)", m.atlas_build_count,
                 m.last_atlas_width);
        overlay_label(line);
    }
    int streaming_on = rogue_asset_manager_streaming_enabled();
    if (overlay_checkbox("Streaming Enabled", &streaming_on))
        rogue_asset_manager_set_streaming_enabled(streaming_on != 0);
    int lazy_on = 0; /* no direct getter; display hint */
    overlay_label("Lazy loading toggle via system or API (no runtime getter) ");
    int prefer_comp = rogue_asset_manager_get_prefer_compressed_textures();
    if (overlay_checkbox("Prefer Compressed Variants", &prefer_comp))
        rogue_asset_manager_set_prefer_compressed_textures(prefer_comp != 0);
    if (overlay_button("Reset Metrics"))
        rogue_asset_manager_reset_metrics();
    overlay_end_panel();
}

void rogue_overlay_register_panel_asset_metrics(void)
{
    overlay_register_panel("asset_metrics", "Asset Metrics", panel_asset_metrics, NULL);
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
