/* asset_browser_atlas_tool.c - extracted Atlas Builder UI */
#include "asset_browser_atlas_tool.h"
#include "../../asset/asset_manager.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "asset_browser_state.h"
#include "asset_browser_util.h" /* future use */

#if ROGUE_ENABLE_DEBUG_OVERLAY
#define g_ab_state (*rogue_asset_browser_state())
void rogue_asset_browser_draw_atlas_tool(RogueAssetManager* m)
{
    (void) m; /* currently only uses global manager via functions */
    overlay_label("[Atlas Builder] Select up to 8 loaded textures (by index) then Build.");
    char buf[64];
    for (int i = 0; i < 8; ++i)
    {
        snprintf(buf, sizeof buf, "TexIdx[%d]", i);
        if (g_ab_state.atlas_selection_count <= i)
            g_ab_state.atlas_selection[i] = -1;
        int val = g_ab_state.atlas_selection[i];
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
#endif
