#include "overlay_theme.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../content/json_io.h"
#include <string.h>

static OverlayTheme g_theme;
static enum OverlayThemePreset g_preset = OVERLAY_THEME_DARK;

static const char* overlay_theme_cfg_path(void) { return "build/overlay_theme.json"; }

static OverlayColor C(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    OverlayColor c = {r, g, b, a};
    return c;
}

static void overlay_theme_apply_preset(enum OverlayThemePreset p)
{
    g_theme.dpi_scale = 1.0f;
    g_theme.font_size = 14;
    switch (p)
    {
    default:
    case OVERLAY_THEME_DARK:
        g_theme.panel_bg = C(10, 10, 14, 180);
        g_theme.panel_border = C(220, 220, 230, 210);
        g_theme.title_text = C(255, 255, 210, 255);
        g_theme.text = C(230, 230, 240, 255);
        g_theme.text_muted = C(180, 180, 200, 255);
        g_theme.text_accent = C(200, 230, 255, 255);
        g_theme.button_bg = C(40, 60, 90, 200);
        g_theme.button_bg_hot = C(60, 80, 120, 210);
        g_theme.button_border = C(200, 200, 220, 220);
        g_theme.button_text = C(255, 255, 255, 255);
        g_theme.checkbox_bg = C(30, 30, 30, 200);
        g_theme.checkbox_border = C(220, 220, 220, 220);
        g_theme.checkbox_tick = C(220, 220, 230, 255);
        g_theme.checkbox_label = C(220, 255, 220, 255);
        g_theme.input_bg = C(25, 25, 25, 200);
        g_theme.input_border = C(220, 220, 220, 220);
        g_theme.input_text = C(255, 255, 255, 255);
        g_theme.table_row_bg = C(20, 20, 20, 200);
        g_theme.table_row_bg_sel = C(60, 80, 120, 220);
        g_theme.table_border = C(220, 220, 220, 220);
        g_theme.table_text = C(255, 255, 255, 255);
        g_theme.toast_info_bg = C(30, 60, 90, 220);
        g_theme.toast_warn_bg = C(100, 80, 20, 230);
        g_theme.toast_error_bg = C(120, 40, 30, 230);
        g_theme.toast_text = C(255, 255, 255, 255);
        g_theme.accent_1 = C(90, 110, 220, 230);
        g_theme.accent_2 = C(60, 160, 200, 255);
        break;
    case OVERLAY_THEME_LIGHT:
        g_theme.panel_bg = C(240, 240, 245, 220);
        g_theme.panel_border = C(40, 40, 60, 220);
        g_theme.title_text = C(20, 20, 20, 255);
        g_theme.text = C(16, 16, 20, 255);
        g_theme.text_muted = C(80, 80, 90, 255);
        g_theme.text_accent = C(20, 80, 140, 255);
        g_theme.button_bg = C(220, 225, 235, 220);
        g_theme.button_bg_hot = C(210, 220, 240, 230);
        g_theme.button_border = C(60, 70, 90, 220);
        g_theme.button_text = C(16, 16, 20, 255);
        g_theme.checkbox_bg = C(230, 230, 235, 220);
        g_theme.checkbox_border = C(60, 60, 80, 220);
        g_theme.checkbox_tick = C(40, 40, 60, 255);
        g_theme.checkbox_label = C(16, 60, 16, 255);
        g_theme.input_bg = C(250, 250, 255, 220);
        g_theme.input_border = C(60, 60, 80, 220);
        g_theme.input_text = C(16, 16, 20, 255);
        g_theme.table_row_bg = C(240, 240, 245, 220);
        g_theme.table_row_bg_sel = C(210, 225, 250, 230);
        g_theme.table_border = C(60, 60, 80, 220);
        g_theme.table_text = C(16, 16, 20, 255);
        g_theme.toast_info_bg = C(200, 230, 255, 230);
        g_theme.toast_warn_bg = C(255, 230, 160, 230);
        g_theme.toast_error_bg = C(255, 180, 160, 230);
        g_theme.toast_text = C(16, 16, 20, 255);
        g_theme.accent_1 = C(40, 80, 160, 230);
        g_theme.accent_2 = C(20, 140, 200, 255);
        break;
    case OVERLAY_THEME_HIGH_CONTRAST:
        g_theme.panel_bg = C(0, 0, 0, 240);
        g_theme.panel_border = C(255, 255, 0, 255);
        g_theme.title_text = C(255, 255, 0, 255);
        g_theme.text = C(255, 255, 255, 255);
        g_theme.text_muted = C(200, 200, 200, 255);
        g_theme.text_accent = C(0, 255, 255, 255);
        g_theme.button_bg = C(0, 0, 0, 255);
        g_theme.button_bg_hot = C(20, 20, 20, 255);
        g_theme.button_border = C(255, 255, 0, 255);
        g_theme.button_text = C(255, 255, 255, 255);
        g_theme.checkbox_bg = C(0, 0, 0, 255);
        g_theme.checkbox_border = C(255, 255, 0, 255);
        g_theme.checkbox_tick = C(255, 255, 0, 255);
        g_theme.checkbox_label = C(255, 255, 0, 255);
        g_theme.input_bg = C(0, 0, 0, 255);
        g_theme.input_border = C(255, 255, 0, 255);
        g_theme.input_text = C(255, 255, 255, 255);
        g_theme.table_row_bg = C(0, 0, 0, 255);
        g_theme.table_row_bg_sel = C(0, 60, 0, 255);
        g_theme.table_border = C(255, 255, 0, 255);
        g_theme.table_text = C(255, 255, 255, 255);
        g_theme.toast_info_bg = C(0, 0, 60, 255);
        g_theme.toast_warn_bg = C(60, 60, 0, 255);
        g_theme.toast_error_bg = C(60, 0, 0, 255);
        g_theme.toast_text = C(255, 255, 255, 255);
        g_theme.accent_1 = C(0, 255, 255, 255);
        g_theme.accent_2 = C(255, 0, 255, 255);
        break;
    }
}

static void overlay_theme_save(void)
{
    char buf[256];
    int n = snprintf(buf, sizeof buf, "{\n  \"preset\": %d,\n  \"dpi\": %.3f,\n  \"font\": %d\n}\n",
                     (int) g_preset, g_theme.dpi_scale, g_theme.font_size);
    char err[128];
    (void) json_io_write_atomic(overlay_theme_cfg_path(), buf, (size_t) n, err, (int) sizeof err);
}

static void overlay_theme_load(void)
{
    char* data = NULL;
    size_t len = 0;
    char err[128];
    if (json_io_read_file(overlay_theme_cfg_path(), &data, &len, err, (int) sizeof err) != 0 ||
        !data)
        return;
    int preset = (int) g_preset;
    float dpi = g_theme.dpi_scale;
    int font = g_theme.font_size;
    /* naive sscanf parse */
    sscanf(data, "%*[^p]preset%*[^0-9]%d%*[^d]dpi%*[^0-9.]%f%*[^f]font%*[^0-9]%d", &preset, &dpi,
           &font);
    if (preset < 0 || preset > 2)
        preset = 0;
    overlay_theme_apply_preset((enum OverlayThemePreset) preset);
    g_preset = (enum OverlayThemePreset) preset;
    if (dpi >= 0.5f && dpi <= 3.0f)
        g_theme.dpi_scale = dpi;
    if (font >= 10 && font <= 28)
        g_theme.font_size = font;
    free(data);
}

void overlay_theme_init(void)
{
    overlay_theme_apply_preset(g_preset);
    overlay_theme_load();
}

const OverlayTheme* overlay_theme_get(void) { return &g_theme; }

void overlay_theme_set_preset(enum OverlayThemePreset p)
{
    g_preset = p;
    overlay_theme_apply_preset(p);
    overlay_theme_save();
}

enum OverlayThemePreset overlay_theme_get_preset(void) { return g_preset; }

void overlay_theme_set_dpi(float scale)
{
    if (scale < 0.5f)
        scale = 0.5f;
    if (scale > 3.0f)
        scale = 3.0f;
    g_theme.dpi_scale = scale;
    overlay_theme_save();
}

float overlay_theme_get_dpi(void) { return g_theme.dpi_scale; }

void overlay_theme_set_font_size(int size)
{
    if (size < 10)
        size = 10;
    if (size > 28)
        size = 28;
    g_theme.font_size = size;
    overlay_theme_save();
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
