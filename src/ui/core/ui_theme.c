/**
 * @file ui_theme.c
 * @brief UI theming system with theme loading, colorblind accessibility support,
 *        and DPI scaling for the roguelike game interface.
 */

#include "ui_theme.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RogueUIThemePack g_active_theme;
static RogueUIColorBlindMode g_cb_mode = ROGUE_COLOR_NORMAL;

/**
 * @brief Parses a hexadecimal color string into a 32-bit RGBA value.
 *
 * Accepts formats like "0xAARRGGBB" or "RRGGBBAA". If only 6 hex digits are provided,
 * treats as RRGGBB with opaque alpha (0xFF).
 *
 * @param s The hexadecimal string to parse.
 * @return The parsed 32-bit RGBA color value.
 */
static uint32_t parse_hex(const char* s)
{
    /* Accept 0xAARRGGBB or RRGGBBAA */
    unsigned v = 0;
    if (!s)
        return 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    while (*s)
    {
        char c = *s++;
        int d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = 10 + c - 'a';
        else if (c >= 'A' && c <= 'F')
            d = 10 + c - 'A';
        else
            break;
        v = (v << 4) | d;
    }
    /* If only 6 hex digits treat as RRGGBB with opaque alpha */
    if (v <= 0xFFFFFFu)
        v = (v << 8) | 0xFFu;
    return v;
}

/**
 * @brief Loads a UI theme from a configuration file.
 *
 * The file format is key=value pairs, one per line. Supported keys include:
 * - Color values: panel_bg, panel_border, text_normal, text_accent, etc.
 * - Numeric values: font_size_base, padding_small, padding_large, dpi_scale_x100
 *
 * @param path Path to the theme configuration file.
 * @param out Pointer to RogueUIThemePack structure to fill.
 * @return 1 on success, 0 on failure.
 */
int rogue_ui_theme_load(const char* path, RogueUIThemePack* out)
{
    if (!path || !out)
        return 0;
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0)
        return 0;
#else
    f = fopen(path, "rb");
    if (!f)
        return 0;
#endif
    char line[256];
    memset(out, 0, sizeof *out);
    out->dpi_scale_x100 = 100;
    while (fgets(line, sizeof line, f))
    {
        char* p = line;
        while (isspace((unsigned char) *p))
            p++;
        if (*p == '#' || *p == 0)
            continue;
        char* eq = strchr(p, '=');
        if (!eq)
            continue;
        *eq = 0;
        char* key = p;
        char* val = eq + 1;
        char* nl = strchr(val, '\n');
        if (nl)
            *nl = 0;
        if (strcmp(key, "panel_bg") == 0)
            out->panel_bg = parse_hex(val);
        else if (strcmp(key, "panel_border") == 0)
            out->panel_border = parse_hex(val);
        else if (strcmp(key, "text_normal") == 0)
            out->text_normal = parse_hex(val);
        else if (strcmp(key, "text_accent") == 0)
            out->text_accent = parse_hex(val);
        else if (strcmp(key, "button_bg") == 0)
            out->button_bg = parse_hex(val);
        else if (strcmp(key, "button_bg_hot") == 0)
            out->button_bg_hot = parse_hex(val);
        else if (strcmp(key, "button_text") == 0)
            out->button_text = parse_hex(val);
        else if (strcmp(key, "slider_track") == 0)
            out->slider_track = parse_hex(val);
        else if (strcmp(key, "slider_fill") == 0)
            out->slider_fill = parse_hex(val);
        else if (strcmp(key, "tooltip_bg") == 0)
            out->tooltip_bg = parse_hex(val);
        else if (strcmp(key, "alert_text") == 0)
            out->alert_text = parse_hex(val);
        else if (strcmp(key, "font_size_base") == 0)
            out->font_size_base = atoi(val);
        else if (strcmp(key, "padding_small") == 0)
            out->padding_small = atoi(val);
        else if (strcmp(key, "padding_large") == 0)
            out->padding_large = atoi(val);
        else if (strcmp(key, "dpi_scale_x100") == 0)
            out->dpi_scale_x100 = atoi(val);
    }
    fclose(f);
    return 1;
}

/**
 * @brief Applies a theme pack as the active UI theme.
 *
 * @param pack Pointer to the theme pack to apply, or NULL to keep current theme.
 */
void rogue_ui_theme_apply(const RogueUIThemePack* pack)
{
    if (pack)
        g_active_theme = *pack;
}

/**
 * @brief Computes a bitmask of differences between two theme packs.
 *
 * @param a First theme pack to compare.
 * @param b Second theme pack to compare.
 * @return Bitmask where each bit represents a differing field.
 */
unsigned int rogue_ui_theme_diff(const RogueUIThemePack* a, const RogueUIThemePack* b)
{
    if (!a || !b)
        return 0;
    unsigned int bits = 0;
    const uint32_t* ap = (const uint32_t*) a;
    const uint32_t* bp = (const uint32_t*) b;
    size_t fields = sizeof(RogueUIThemePack) / sizeof(uint32_t);
    for (size_t i = 0; i < fields; i++)
    {
        if (ap[i] != bp[i])
            bits |= (1u << i);
    }
    return bits;
}

/**
 * @brief Sets the colorblind accessibility mode.
 *
 * @param mode The colorblind mode to set.
 */
void rogue_ui_colorblind_set_mode(RogueUIColorBlindMode mode) { g_cb_mode = mode; }

/**
 * @brief Gets the current colorblind accessibility mode.
 *
 * @return The current colorblind mode.
 */
RogueUIColorBlindMode rogue_ui_colorblind_mode(void) { return g_cb_mode; }

/**
 * @brief Transforms a color for colorblind accessibility.
 *
 * Applies linear color transformation matrices to simulate various types of
 * color vision deficiencies based on Machado et al. 2009 research.
 *
 * @param rgba The input RGBA color value.
 * @return The transformed RGBA color value.
 */
uint32_t rogue_ui_colorblind_transform(uint32_t rgba)
{
    unsigned r = (rgba >> 24) & 0xFFu, g = (rgba >> 16) & 0xFFu, b = (rgba >> 8) & 0xFFu,
             a = rgba & 0xFFu;
    float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
    float nr = rf, ng = gf, nb = bf;
    switch (g_cb_mode)
    {
    case ROGUE_COLOR_PROTANOPIA: /* weak / missing L cones */
        nr = 0.566f * rf + 0.433f * gf + 0.0f * bf;
        ng = 0.558f * rf + 0.442f * gf + 0.0f * bf;
        nb = 0.0f * rf + 0.242f * gf + 0.758f * bf;
        break;
    case ROGUE_COLOR_DEUTERANOPIA: /* weak / missing M cones */
        nr = 0.625f * rf + 0.375f * gf + 0.0f * bf;
        ng = 0.7f * rf + 0.3f * gf + 0.0f * bf;
        nb = 0.0f * rf + 0.3f * gf + 0.7f * bf;
        break;
    case ROGUE_COLOR_TRITANOPIA: /* weak / missing S cones */
        nr = 0.95f * rf + 0.05f * gf + 0.0f * bf;
        ng = 0.0f * rf + 0.433f * gf + 0.567f * bf;
        nb = 0.0f * rf + 0.475f * gf + 0.525f * bf;
        break;
    default:
        break; /* normal vision no change */
    }
    if (nr < 0)
        nr = 0;
    if (ng < 0)
        ng = 0;
    if (nb < 0)
        nb = 0;
    if (nr > 1)
        nr = 1;
    if (ng > 1)
        ng = 1;
    if (nb > 1)
        nb = 1;
    r = (unsigned) (nr * 255.0f + 0.5f);
    g = (unsigned) (ng * 255.0f + 0.5f);
    b = (unsigned) (nb * 255.0f + 0.5f);
    return (r << 24) | (g << 16) | (b << 8) | a;
}

/**
 * @brief Gets the current DPI scaling factor multiplied by 100.
 *
 * @return DPI scale factor * 100 (e.g., 150 for 1.5x scaling).
 */
int rogue_ui_dpi_scale_x100(void)
{
    return g_active_theme.dpi_scale_x100 ? g_active_theme.dpi_scale_x100 : 100;
}

/**
 * @brief Scales a pixel value according to the current DPI scaling.
 *
 * @param px The pixel value to scale.
 * @return The scaled pixel value.
 */
int rogue_ui_scale_px(int px)
{
    int s = rogue_ui_dpi_scale_x100();
    return (px * s + 50) / 100;
}

/**
 * @brief Sets the DPI scaling factor.
 *
 * @param value The new DPI scale factor * 100 (clamped to 50-300 range).
 */
void rogue_ui_theme_set_dpi_scale_x100(int value)
{
    if (value < 50)
        value = 50; /* clamp to sensible minimum 0.5x */
    if (value > 300)
        value = 300; /* clamp to 3.0x to avoid runaway sizes */
    g_active_theme.dpi_scale_x100 = value;
}
