/* Phase 7.1-7.3 UI Theme & Accessibility */
#ifndef ROGUE_UI_THEME_H
#define ROGUE_UI_THEME_H
#include <stddef.h>
#include <stdint.h>

typedef struct RogueUIThemePack
{
    /* Core palette */
    uint32_t panel_bg;
    uint32_t panel_border;
    uint32_t text_normal;
    uint32_t text_accent;
    uint32_t button_bg;
    uint32_t button_bg_hot;
    uint32_t button_text;
    uint32_t slider_track;
    uint32_t slider_fill;
    uint32_t tooltip_bg;
    uint32_t alert_text;
    int font_size_base;
    int padding_small;
    int padding_large;
    int dpi_scale_x100; /* scaled by 100 for integer determinism (e.g., 125=1.25x) */
} RogueUIThemePack;

/* Load theme pack from kv file (key=value lines). Returns 0 on failure. */
int rogue_ui_theme_load(const char* path, RogueUIThemePack* out);
/* Apply theme to active UI context (updates global style variables). */
void rogue_ui_theme_apply(const RogueUIThemePack* pack);
/* Diff two theme packs; returns bitmask of changed fields (bit index per field order above). */
unsigned int rogue_ui_theme_diff(const RogueUIThemePack* a, const RogueUIThemePack* b);

/* Accessibility (Phase 7.3): colorblind remap modes */
typedef enum RogueUIColorBlindMode
{
    ROGUE_COLOR_NORMAL = 0,
    ROGUE_COLOR_PROTANOPIA = 1,
    ROGUE_COLOR_DEUTERANOPIA = 2,
    ROGUE_COLOR_TRITANOPIA = 3
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
