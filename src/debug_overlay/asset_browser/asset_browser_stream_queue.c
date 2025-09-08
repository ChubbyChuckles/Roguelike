/* asset_browser_stream_queue.c
   Extracted streaming queue UI from panels_asset_browser.c */

#include "debug_overlay/asset_browser/asset_browser_stream_queue.h"
#include "../widgets/overlay_widgets.h"
#include "asset/asset_manager.h"
#include "debug_overlay/asset_browser/asset_browser_state.h"

#define g_ab_state (*rogue_asset_browser_state())

#if ROGUE_ENABLE_DEBUG_OVERLAY
static void overlay_same_line(void) { /* no-op layout shim */ }
#endif

void rogue_asset_browser_draw_stream_queue(RogueAssetManager* m)
{
    (void) g_ab_state; /* state flags handled by caller */
    overlay_label("[Streaming Queue]");
    int enabled = rogue_asset_manager_streaming_enabled();
    if (overlay_checkbox("Streaming Enabled", &enabled))
        rogue_asset_manager_set_streaming_enabled(enabled ? 1 : 0);
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
            (void) tex; /* currently unused – placeholder for future preview */
            const char* state =
                jobs[i].already_loaded ? "loaded" : (jobs[i].load_failed ? "failed" : "pending");
            char line2[340];
            snprintf(line2, sizeof line2, "%2d | %6d | %-7s | %s", i, jobs[i].texture_index, state,
                     jobs[i].path);
            overlay_label(line2);
        }
        overlay_label("(Jobs load in reverse insertion order – compact removal)");
    }
    else
    {
        overlay_label("Queue empty. Enqueue via gameplay systems or add future test UI.");
    }
}
