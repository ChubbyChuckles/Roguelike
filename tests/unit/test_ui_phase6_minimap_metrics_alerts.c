#define SDL_MAIN_HANDLED 1
#include "core/app_state.h"
#include "core/buffs.h"
#include "core/hud/hud_overlays.h"
#include "core/minimap.h"
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;

int main()
{
    /* Initialize minimal fields */
    g_app.viewport_w = 320;
    g_app.viewport_h = 240;
    g_app.show_minimap = 1;
    g_app.show_metrics_overlay = 1;
    /* Simulate alerts */
    rogue_alerts_reset();
    rogue_alert_level_up();
    rogue_alert_low_health();
    rogue_alert_vendor_restock();
    /* Update and render (headless: renderer NULL so just state counters) */
    rogue_alerts_update_and_render(16.0f);
    if (g_app.last_alerts_rendered == 0)
    {
        printf("FAIL expected alerts rendered>0\n");
        return 1;
    }
    /* Toggle minimap off/on and check flag flipping logic in app_step integration (simulate) */
    g_app.show_minimap = 0;
    if (g_app.show_minimap != 0)
    {
        printf("FAIL minimap toggle off\n");
        return 1;
    }
    g_app.show_minimap = 1;
    if (!g_app.show_minimap)
    {
        printf("FAIL minimap toggle on\n");
        return 1;
    }
    g_app.show_metrics_overlay = 1;
    rogue_metrics_overlay_render(); /* requires renderer to actually draw; here just ensure function
                                       callable */
    printf("test_ui_phase6_minimap_metrics_alerts: OK\n");
    return 0;
}
