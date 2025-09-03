#include "../../core/app/app_state.h"
#include "../overlay_core.h"
#include "../overlay_theme.h"
#include "../widgets/overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_system(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("system", "System", 10, 10, 320))
        return;
    char buf[128];
    double fps = 0.0, fms = 0.0, avg = 0.0;
#ifdef __has_include
#if __has_include("../util/metrics.h")
#include "../../util/metrics.h"
    rogue_metrics_get(&fps, &fms, &avg);
#endif
#endif
    if (fps <= 0.0)
    {
        float dt = overlay_last_dt();
        fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;
        fms = g_app.frame_ms;
    }
    snprintf(buf, sizeof(buf), "FPS: %.1f  (%.3f ms)", fps, fms);
    overlay_label(buf);
    snprintf(buf, sizeof(buf), "Draw calls: %d", g_app.frame_draw_calls);
    overlay_label(buf);
    snprintf(buf, sizeof(buf), "Tile quads: %d", g_app.frame_tile_quads);
    overlay_label(buf);
    int flags = g_app.show_metrics_overlay ? 1 : 0;
    if (overlay_checkbox("Show metrics overlay (F1)", &flags))
    {
        g_app.show_metrics_overlay = flags;
        overlay_set_enabled(flags);
    }

    /* Theme controls */
    overlay_label("Theme");
    const char* theme_items[] = {"Dark", "Light", "High Contrast"};
    int preset = (int) overlay_theme_get_preset();
    if (overlay_combo("Preset", &preset, theme_items, 3))
    {
        overlay_theme_set_preset((enum OverlayThemePreset) preset);
    }
    int cb = overlay_theme_get_colorblind();
    if (overlay_checkbox("Colorblind-safe palette", &cb))
    {
        overlay_theme_set_colorblind(cb);
    }
    float dpi = overlay_theme_get_dpi();
    if (overlay_slider_float("DPI Scale", &dpi, 0.5f, 3.0f))
    {
        overlay_theme_set_dpi(dpi);
    }
    const OverlayTheme* th = overlay_theme_get();
    int fsz = th->font_size;
    if (overlay_slider_int("Font Size", &fsz, 10, 28))
    {
        overlay_theme_set_font_size(fsz);
    }
    overlay_end_panel();
}

void rogue_overlay_register_panel_system(void)
{
    overlay_register_panel("system", "System", panel_system, NULL);
}

#endif
