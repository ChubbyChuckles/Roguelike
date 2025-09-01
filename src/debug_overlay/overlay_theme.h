#ifndef ROGUE_DEBUG_OVERLAY_THEME_H
#define ROGUE_DEBUG_OVERLAY_THEME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct OverlayColor
    {
        uint8_t r, g, b, a;
    } OverlayColor;

    typedef struct OverlayTheme
    {
        /* Panel */
        OverlayColor panel_bg;
        OverlayColor panel_border;
        OverlayColor title_text;
        /* Text */
        OverlayColor text;
        OverlayColor text_muted;
        OverlayColor text_accent;
        /* Buttons */
        OverlayColor button_bg;
        OverlayColor button_bg_hot;
        OverlayColor button_border;
        OverlayColor button_text;
        /* Checkbox */
        OverlayColor checkbox_bg;
        OverlayColor checkbox_border;
        OverlayColor checkbox_tick;
        OverlayColor checkbox_label;
        /* Inputs */
        OverlayColor input_bg;
        OverlayColor input_border;
        OverlayColor input_text;
        /* Table */
        OverlayColor table_row_bg;
        OverlayColor table_row_bg_sel;
        OverlayColor table_border;
        OverlayColor table_text;
        /* Toasts */
        OverlayColor toast_info_bg;
        OverlayColor toast_warn_bg;
        OverlayColor toast_error_bg;
        OverlayColor toast_text;
        /* Accents */
        OverlayColor accent_1;
        OverlayColor accent_2;
        /* Metrics */
        float dpi_scale; /* 1.0 = default */
        int font_size;   /* base font size ladder */
    } OverlayTheme;

    enum OverlayThemePreset
    {
        OVERLAY_THEME_DARK = 0,
        OVERLAY_THEME_LIGHT = 1,
        OVERLAY_THEME_HIGH_CONTRAST = 2,
    };

    /* Initialize theme system, loading persisted choice if any */
    void overlay_theme_init(void);
    /* Get/Set theme presets; setting persists to disk. */
    const OverlayTheme* overlay_theme_get(void);
    void overlay_theme_set_preset(enum OverlayThemePreset p);
    enum OverlayThemePreset overlay_theme_get_preset(void);
    /* DPI helpers (clamp to sane ranges). */
    void overlay_theme_set_dpi(float scale);
    float overlay_theme_get_dpi(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_DEBUG_OVERLAY_THEME_H */
