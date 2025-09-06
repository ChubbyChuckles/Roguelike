/**
 * @file loot_drop_rates.c
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 *
 * This file manages drop rate scalars for global and per-item-category adjustments in loot
 * generation. Uses lazy initialization (ensure_init) to set defaults to 1.0. Supports
 * setting/retrieving global scalar (clamped >=0), and per-category scalars (clamped >=0,
 * defaults 1.0 for valid categories 0 to ROGUE_ITEM__COUNT-1). Reset clears all to 1.0. Used to
 * balance drop probabilities without altering base weights.
 */

#include "loot_drop_rates.h"
#include "loot_item_defs.h"

static float g_global_scalar = 1.0f;
static float g_category_scalar[ROGUE_ITEM__COUNT];
static int g_drop_rates_inited = 0;

/**
 * @brief Lazy initialization helper for drop rate scalars.
 *
 * Sets all category scalars to 1.0 if not already initialized.
 */
static void ensure_init(void)
{
    if (!g_drop_rates_inited)
    {
        for (int i = 0; i < ROGUE_ITEM__COUNT; i++)
            g_category_scalar[i] = 1.0f;
        g_drop_rates_inited = 1;
    }
}

/**
 * @brief Resets all drop rate scalars to neutral (1.0).
 *
 * Sets global and all category scalars to 1.0, marks as initialized.
 */
void rogue_drop_rates_reset(void)
{
    g_global_scalar = 1.0f;
    for (int i = 0; i < ROGUE_ITEM__COUNT; i++)
        g_category_scalar[i] = 1.0f;
    g_drop_rates_inited = 1;
}

/**
 * @brief Sets the global drop rate multiplier.
 *
 * Ensures init, clamps scalar >=0.
 *
 * @param scalar The new global scalar (clamped >=0).
 */
void rogue_drop_rates_set_global(float scalar)
{
    ensure_init();
    if (scalar < 0.0f)
        scalar = 0.0f;
    g_global_scalar = scalar;
}
/**
 * @brief Retrieves the current global drop rate scalar.
 *
 * Ensures init first.
 *
 * @return The global scalar (default 1.0 if not set).
 */
float rogue_drop_rates_get_global(void)
{
    ensure_init();
    return g_global_scalar;
}

/**
 * @brief Sets the drop rate scalar for a specific item category.
 *
 * Ensures init, ignores invalid category, clamps scalar >=0.
 *
 * @param category The item category index (0 to ROGUE_ITEM__COUNT-1).
 * @param scalar The new category scalar (clamped >=0).
 */
void rogue_drop_rates_set_category(int category, float scalar)
{
    ensure_init();
    if (category < 0 || category >= ROGUE_ITEM__COUNT)
        return;
    if (scalar < 0.0f)
        scalar = 0.0f;
    g_category_scalar[category] = scalar;
}
/**
 * @brief Retrieves the drop rate scalar for a specific item category.
 *
 * Ensures init, returns 1.0 for invalid category.
 *
 * @param category The item category index.
 * @return The category scalar (default 1.0 if invalid or not set).
 */
float rogue_drop_rates_get_category(int category)
{
    ensure_init();
    if (category < 0 || category >= ROGUE_ITEM__COUNT)
        return 1.0f;
    return g_category_scalar[category];
}
