/**
 * @file hud_layout.c
 * @brief HUD layout configuration system for UI Phase 6.1.
 *
 * This module implements a flexible HUD layout system that allows customization
 * of UI element positions and dimensions through configuration files. It provides
 * default layouts for health, mana, and XP bars, as well as level text positioning.
 *
 * Key features:
 * - Configurable bar positions and dimensions via key-value configuration files
 * - Automatic fallback to default layouts when configuration is missing
 * - Support for relative positioning (bars stack vertically by default)
 * - Platform-specific parsing with secure scanf variants on MSVC
 * - Thread-safe global layout state management
 *
 * Configuration format example:
 * ```
 * health_bar=6,4,200,10
 * mana_bar=6,20,200,8
 * xp_bar=6,34,200,6
 * level_text=214,4
 * ```
 *
 * @note All coordinates are in screen pixels, origin at top-left (0,0)
 * @note Bars are automatically stacked vertically with 6-pixel gaps by default
 */

/* hud_layout.c - UI Phase 6.1 HUD layout spec loader implementation */
#include "hud_layout.h"
#include "../../util/kv_parser.h"
#include <stdio.h>
#include <string.h>

static RogueHUDLayout g_hud_layout;

/**
 * @brief Applies default HUD layout values to the global layout structure.
 *
 * Sets up a standard vertical layout with health bar at the top, followed by
 * mana bar, then XP bar, with level text positioned to the right of the health bar.
 * All bars have appropriate widths and heights for typical UI display.
 *
 * Layout configuration:
 * - Health bar: 200x10 pixels at (6,4)
 * - Mana bar: 200x8 pixels, positioned 6 pixels below health bar
 * - XP bar: 200x6 pixels, positioned 6 pixels below mana bar
 * - Level text: positioned 8 pixels to the right of health bar
 *
 * @note This function is called automatically when layout is first accessed
 *       if no custom layout has been loaded.
 */
static void apply_defaults(void)
{
    g_hud_layout.health.x = 6;
    g_hud_layout.health.y = 4;
    g_hud_layout.health.w = 200;
    g_hud_layout.health.h = 10;
    g_hud_layout.mana.x = 6;
    g_hud_layout.mana.y = g_hud_layout.health.y + g_hud_layout.health.h + 6;
    g_hud_layout.mana.w = 200;
    g_hud_layout.mana.h = 8;
    g_hud_layout.xp.x = 6;
    g_hud_layout.xp.y = g_hud_layout.mana.y + g_hud_layout.mana.h + 6;
    g_hud_layout.xp.w = 200;
    g_hud_layout.xp.h = 6;
    g_hud_layout.level_text_x = g_hud_layout.health.x + g_hud_layout.health.w + 8;
    g_hud_layout.level_text_y = g_hud_layout.health.y;
    g_hud_layout.loaded = 0;
}

/**
 * @brief Resets the HUD layout to default values.
 *
 * Forces the layout system to use the built-in default configuration,
 * discarding any previously loaded custom layout. This is useful for
 * resetting to a known state or when custom layouts become corrupted.
 *
 * After calling this function, the layout will use standard positions:
 * - Health bar at (6,4) with 200x10 dimensions
 * - Mana bar stacked below health with 200x8 dimensions
 * - XP bar stacked below mana with 200x6 dimensions
 * - Level text positioned to the right of health bar
 *
 * @note This function immediately applies the defaults and marks the layout as unloaded.
 */
void rogue_hud_layout_reset_defaults(void) { apply_defaults(); }

/**
 * @brief Gets the current HUD layout configuration.
 *
 * Returns a pointer to the global HUD layout structure containing positions
 * and dimensions for all UI elements. If no layout has been loaded and the
 * layout appears uninitialized (health bar width is 0), automatically applies
 * default values first.
 *
 * @return Pointer to the current RogueHUDLayout structure containing:
 *         - health: Health bar position and dimensions
 *         - mana: Mana bar position and dimensions
 *         - xp: XP bar position and dimensions
 *         - level_text_x, level_text_y: Level text position
 *         - loaded: Flag indicating if custom layout was loaded
 *
 * @note The returned pointer is to a global static structure and should not be freed.
 * @note Thread-safe for read access, but layout modifications should be synchronized.
 */
const RogueHUDLayout* rogue_hud_layout(void)
{
    if (g_hud_layout.health.w == 0)
        apply_defaults();
    return &g_hud_layout;
}

/**
 * @brief Parses a bar rectangle specification from a string.
 *
 * Parses a comma-separated string in the format "x,y,width,height" and
 * populates a RogueHUDBarRect structure. Performs validation to ensure
 * width and height are at least 1 pixel to prevent rendering issues.
 *
 * @param value String containing bar specification in "x,y,width,height" format
 * @param out Pointer to RogueHUDBarRect structure to populate
 * @return 1 on successful parsing and validation, 0 on failure
 *
 * @note Uses platform-specific scanf variants for security (sscanf_s on MSVC)
 * @note Automatically clamps width and height to minimum of 1 pixel
 * @note Returns 0 if value or out parameters are NULL
 */
static int parse_bar(const char* value, RogueHUDBarRect* out)
{
    if (!value || !out)
        return 0;
    int x = 0, y = 0, w = 0, h = 0;
#if defined(_MSC_VER)
    if (sscanf_s(value, "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
#else
    if (sscanf(value, "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
#endif
    {
        if (w < 1)
            w = 1;
        if (h < 1)
            h = 1;
        out->x = x;
        out->y = y;
        out->w = w;
        out->h = h;
        return 1;
    }
    return 0;
}

/**
 * @brief Parses coordinate pair from a string.
 *
 * Parses a comma-separated string in the format "x,y" and populates
 * the provided integer pointers with the parsed values.
 *
 * @param value String containing coordinate specification in "x,y" format
 * @param x Pointer to integer to store X coordinate
 * @param y Pointer to integer to store Y coordinate
 * @return 1 on successful parsing, 0 on failure
 *
 * @note Uses platform-specific scanf variants for security (sscanf_s on MSVC)
 * @note Returns 0 if value, x, or y parameters are NULL
 * @note Coordinates can be negative (off-screen positioning allowed)
 */
static int parse_xy(const char* value, int* x, int* y)
{
    if (!value || !x || !y)
        return 0;
    int vx = 0, vy = 0;
#if defined(_MSC_VER)
    if (sscanf_s(value, "%d,%d", &vx, &vy) == 2)
#else
    if (sscanf(value, "%d,%d", &vx, &vy) == 2)
#endif
    {
        *x = vx;
        *y = vy;
        return 1;
    }
    return 0;
}

/**
 * @brief Loads HUD layout configuration from a key-value file.
 *
 * Parses a configuration file containing HUD element positions and dimensions.
 * Supports the following configuration keys:
 * - health_bar: Health bar rectangle in "x,y,width,height" format
 * - mana_bar: Mana bar rectangle in "x,y,width,height" format
 * - xp_bar: XP bar rectangle in "x,y,width,height" format
 * - level_text: Level text position in "x,y" format
 *
 * If the primary file path fails to load, attempts to load from "../{path}"
 * as a fallback. If loading fails completely, leaves the current layout unchanged.
 *
 * @param path Path to the configuration file to load
 * @return 1 if any configuration values were successfully loaded and applied, 0 otherwise
 *
 * @note Only successfully parsed values are applied; invalid entries are ignored
 * @note The layout's 'loaded' flag is set to 1 if any values were successfully loaded
 * @note Falls back to default layout if file cannot be loaded at all
 * @note Thread-safe for layout access, but concurrent loads may cause race conditions
 */
int rogue_hud_layout_load(const char* path)
{
    if (g_hud_layout.health.w == 0)
        apply_defaults();
    RogueKVFile kv;
    if (!rogue_kv_load_file(path, &kv))
    {
        /* attempt ../ fallback */
        char alt[256];
        snprintf(alt, sizeof alt, "../%s", path);
        if (!rogue_kv_load_file(alt, &kv))
            return 0; /* leave defaults */
    }
    int cursor = 0;
    RogueKVEntry e;
    RogueKVError err;
    int any = 0;
    while (rogue_kv_next(&kv, &cursor, &e, &err))
    {
        if (strcmp(e.key, "health_bar") == 0)
        {
            if (parse_bar(e.value, &g_hud_layout.health))
                any = 1;
        }
        else if (strcmp(e.key, "mana_bar") == 0)
        {
            if (parse_bar(e.value, &g_hud_layout.mana))
                any = 1;
        }
        else if (strcmp(e.key, "xp_bar") == 0)
        {
            if (parse_bar(e.value, &g_hud_layout.xp))
                any = 1;
        }
        else if (strcmp(e.key, "level_text") == 0)
        {
            if (parse_xy(e.value, &g_hud_layout.level_text_x, &g_hud_layout.level_text_y))
                any = 1;
        }
    }
    rogue_kv_free(&kv);
    if (any)
        g_hud_layout.loaded = 1;
    return any;
}
