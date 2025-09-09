/**
 * @file ui_theme.h
 * @brief UI theming and accessibility system.
 *
 * Provides comprehensive UI theming support with color palettes, font sizing,
 * padding configurations, and accessibility features including colorblind
 * support. Supports loading themes from configuration files and dynamic
 * theme switching with difference detection for efficient updates.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

#ifndef ROGUE_UI_THEME_H
#define ROGUE_UI_THEME_H
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Complete UI theme configuration.
 *
 * Contains all visual styling parameters for the user interface
 * including colors, fonts, spacing, and scaling. Colors are stored
 * as packed 32-bit ARGB values for efficient rendering.
 */
typedef struct RogueUIThemePack
{
    /* Color palette */
    uint32_t panel_bg;       ///< Panel background color (ARGB)
    uint32_t panel_border;   ///< Panel border color (ARGB)
    uint32_t text_normal;    ///< Normal text color (ARGB)
    uint32_t text_accent;    ///< Accent/highlighted text color (ARGB)
    uint32_t button_bg;      ///< Button background color (ARGB)
    uint32_t button_bg_hot;  ///< Button hover/active background color (ARGB)
    uint32_t button_text;    ///< Button text color (ARGB)
    uint32_t slider_track;   ///< Slider track color (ARGB)
    uint32_t slider_fill;    ///< Slider fill color (ARGB)
    uint32_t tooltip_bg;     ///< Tooltip background color (ARGB)
    uint32_t alert_text;     ///< Alert/warning text color (ARGB)
    
    /* Typography and spacing */
    int font_size_base;      ///< Base font size in pixels
    int padding_small;       ///< Small padding value in pixels
    int padding_large;       ///< Large padding value in pixels
    int dpi_scale_x100;      ///< DPI scaling factor * 100 (e.g., 125 = 1.25x scale)
} RogueUIThemePack;

/**
 * @brief Load theme configuration from file.
 *
 * Loads a theme pack from a key-value configuration file.
 * The file format uses simple "key=value" lines for each
 * theme parameter.
 *
 * @param path Path to the theme configuration file
 * @param out Pointer to theme pack structure to populate
 * @return Non-zero on success, 0 on failure
 */
int rogue_ui_theme_load(const char* path, RogueUIThemePack* out);

/**
 * @brief Apply theme to the active UI context.
 *
 * Updates the global UI style variables with the values from
 * the specified theme pack. This affects all subsequent UI
 * rendering operations.
 *
 * @param pack Pointer to the theme pack to apply
 */
void rogue_ui_theme_apply(const RogueUIThemePack* pack);

/**
 * @brief Compare two theme packs and detect differences.
 *
 * Performs a field-by-field comparison of two theme packs and
 * returns a bitmask indicating which fields differ. Each bit
 * corresponds to a field in the structure order.
 *
 * @param a First theme pack to compare
 * @param b Second theme pack to compare
 * @return Bitmask of changed fields (bit index matches field order)
 */
unsigned int rogue_ui_theme_diff(const RogueUIThemePack* a, const RogueUIThemePack* b);

/**
 * @brief Colorblind accessibility modes.
 *
 * Defines different types of color vision deficiency that can be
 * compensated for by the UI theming system.
 */
typedef enum RogueUIColorBlindMode
{
    ROGUE_COLOR_NORMAL = 0,       ///< Normal color vision
    ROGUE_COLOR_PROTANOPIA = 1,   ///< Red-green colorblind (no red receptors)
    ROGUE_COLOR_DEUTERANOPIA = 2, ///< Red-green colorblind (no green receptors)
    ROGUE_COLOR_TRITANOPIA = 3    ///< Blue-yellow colorblind (no blue receptors)
} RogueUIColorBlindMode;
void rogue_ui_colorblind_set_mode(RogueUIColorBlindMode mode);
RogueUIColorBlindMode rogue_ui_colorblind_mode(void);
/* Transform a color according to current colorblind mode (for tests). */
uint32_t rogue_ui_colorblind_transform(uint32_t rgba);

/* DPI scaling helpers (Phase 7.4) */
int rogue_ui_dpi_scale_x100(void); /* returns active theme dpi_scale_x100 or 100 if none */
int rogue_ui_scale_px(int px);     /* scales integer pixel dimension with rounding */

/* Phase 6.3: allow runtime DPI scale adjustment without reloading a theme */
void rogue_ui_theme_set_dpi_scale_x100(int value);

#endif /* ROGUE_UI_THEME_H */
