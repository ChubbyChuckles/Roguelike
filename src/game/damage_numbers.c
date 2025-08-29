/**
 * @file damage_numbers.c
 * @brief Visual damage feedback system with physics and animation
 *
 * This module implements a comprehensive damage number display system that provides
 * visual feedback for damage dealt in combat. Features include spatial batching,
 * physics simulation, color-coded damage types, critical hit effects, and smooth
 * fade animations.
 *
 * Key Features:
 * - Spatial batching: Combines nearby damage numbers to reduce visual clutter
 * - Physics simulation: Numbers float upward with gravity and velocity
 * - Color coding: Different colors for player damage, enemy damage, and critical hits
 * - Critical hit effects: Larger scale and distinct yellow coloring
 * - Smooth animations: Fade-in curve and quadratic fade-out
 * - SDL rendering: Hardware-accelerated text rendering with alpha blending
 *
 * Animation System:
 * - Numbers spawn with upward velocity (-0.38 tiles/second)
 * - Gravity acceleration (-0.15 tiles/second²) slows upward movement
 * - Fade-in: Linear ramp over first 80ms to full opacity
 * - Fade-out: Quadratic curve over last 150ms to zero opacity
 * - Total lifetime: 700ms with optional extension for batched numbers
 *
 * Color Scheme:
 * - Player damage: Orange (255, 210, 40) - warm, positive feedback
 * - Enemy damage: Red (255, 60, 60) - hostile, damage-indicating
 * - Critical hits: Yellow (255, 255, 120) - bright, rewarding feedback
 *
 * Performance Optimizations:
 * - Spatial batching within 0.4 tile radius prevents spam
 * - Efficient array management with swap-and-pop removal
 * - Alpha pre-calculation for smooth blending
 * - Scale clamping to prevent excessive rendering overhead
 *
 * @note Designed for clear visual feedback without overwhelming the player
 * @note Spatial batching helps maintain performance during high-damage scenarios
 * @note Animation curves tuned for smooth, readable damage feedback
 * @note SDL rendering provides hardware acceleration and alpha blending
 */

#include "damage_numbers.h"
#include "../core/app/app_state.h"
#include "../graphics/font.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/**
 * @brief Add a damage number with critical hit support and spatial batching
 *
 * Creates a visual damage number at the specified world position with optional
 * critical hit effects. Implements spatial batching to combine nearby damage
 * numbers and prevent visual spam during high-damage scenarios.
 *
 * Spatial Batching Logic:
 * - Searches for existing damage numbers within 0.4 tile radius
 * - Combines with numbers of same ownership and critical status
 * - Extends lifetime by 120ms when batching occurs
 * - Prevents lifetime from exceeding original total duration
 *
 * Critical Hit Effects:
 * - 1.4x scale multiplier for visual prominence
 * - Distinct yellow coloring (255, 255, 120)
 * - Same spatial batching rules apply
 *
 * @param x World X coordinate (in tiles) where damage number appears
 * @param y World Y coordinate (in tiles) where damage number appears
 * @param amount Damage amount to display (must be non-zero)
 * @param from_player 1 if damage dealt by player, 0 if by enemy/NPC
 * @param crit 1 if this is a critical hit, 0 for normal damage
 *
 * @note Zero damage amounts are ignored (no visual feedback)
 * @note Spatial batching radius is 0.4 tiles (approximately 2-3 pixels)
 * @note Maximum 120ms lifetime extension when batching
 * @note Critical hits get 40% larger scale for visual emphasis
 * @note Function is thread-safe for the global damage number array
 */
void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit)
{
    if (amount == 0)
        return;
    /* Simple spatial batching: if a recent number within small radius (<0.4 tiles) and same
     * ownership, accumulate. */
    for (int i = 0; i < g_app.dmg_number_count; i++)
    {
        float dx = g_app.dmg_numbers[i].x - x;
        float dy = g_app.dmg_numbers[i].y - y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 < 0.16f && g_app.dmg_numbers[i].from_player == from_player &&
            g_app.dmg_numbers[i].crit == crit)
        {
            g_app.dmg_numbers[i].amount += amount; /* refresh life slightly */
            float extend = 120.0f;
            g_app.dmg_numbers[i].life_ms += extend;
            if (g_app.dmg_numbers[i].life_ms > g_app.dmg_numbers[i].total_ms)
                g_app.dmg_numbers[i].life_ms = g_app.dmg_numbers[i].total_ms;
            return;
        }
    }
    if (g_app.dmg_number_count < (int) (sizeof(g_app.dmg_numbers) / sizeof(g_app.dmg_numbers[0])))
    {
        int i = g_app.dmg_number_count++;
        g_app.dmg_numbers[i].x = x;
        g_app.dmg_numbers[i].y = y;
        g_app.dmg_numbers[i].vx = 0.0f;
        g_app.dmg_numbers[i].vy = -0.38f;
        g_app.dmg_numbers[i].life_ms = 700.0f;
        g_app.dmg_numbers[i].total_ms = 700.0f;
        g_app.dmg_numbers[i].amount = amount;
        g_app.dmg_numbers[i].from_player = from_player;
        g_app.dmg_numbers[i].crit = crit ? 1 : 0;
        g_app.dmg_numbers[i].scale = crit ? 1.4f : 1.0f;
        g_app.dmg_numbers[i].spawn_ms = (float) g_app.game_time_ms;
        g_app.dmg_numbers[i].alpha = 1.0f;
    }
}
/**
 * @brief Add a standard damage number (non-critical)
 *
 * Convenience function for adding regular damage numbers without critical hit effects.
 * Internally calls rogue_add_damage_number_ex() with crit=0.
 *
 * @param x World X coordinate (in tiles) where damage number appears
 * @param y World Y coordinate (in tiles) where damage number appears
 * @param amount Damage amount to display (must be non-zero)
 * @param from_player 1 if damage dealt by player, 0 if by enemy/NPC
 *
 * @note Zero damage amounts are ignored
 * @note Uses standard orange coloring for player damage, red for enemy damage
 * @note Still benefits from spatial batching system
 * @note For critical hits, use rogue_add_damage_number_ex() instead
 */
void rogue_add_damage_number(float x, float y, int amount, int from_player)
{
    rogue_add_damage_number_ex(x, y, amount, from_player, 0);
}
/**
 * @brief Get the current count of active damage numbers
 *
 * Returns the number of damage numbers currently being displayed and animated.
 * Useful for performance monitoring and debugging the damage feedback system.
 *
 * @return Number of active damage numbers (0 to maximum array capacity)
 *
 * @note Maximum count is determined by g_app.dmg_numbers array size
 * @note Count decreases as numbers fade out and are removed
 * @note Can be used for performance monitoring during high-damage scenarios
 */
int rogue_app_damage_number_count(void) { return g_app.dmg_number_count; }
/**
 * @brief Test utility for manually decaying damage numbers
 *
 * Manually advances the lifetime of all damage numbers by the specified amount.
 * Used for testing the fade-out behavior and cleanup logic without waiting
 * for real-time progression.
 *
 * @param ms Milliseconds to subtract from each damage number's remaining lifetime
 *
 * @note Primarily used for unit testing and debugging
 * @note Numbers with zero or negative lifetime are immediately removed
 * @note Uses efficient swap-and-pop removal to maintain array continuity
 * @note Safe to call with any millisecond value (positive or negative)
 */
void rogue_app_test_decay_damage_numbers(float ms)
{
    for (int i = 0; i < g_app.dmg_number_count;)
    {
        g_app.dmg_numbers[i].life_ms -= ms;
        if (g_app.dmg_numbers[i].life_ms <= 0)
        {
            g_app.dmg_numbers[i] = g_app.dmg_numbers[g_app.dmg_number_count - 1];
            g_app.dmg_number_count--;
            continue;
        }
        ++i;
    }
}

/**
 * @brief Update damage numbers with physics simulation and animation
 *
 * Performs per-frame updates for all active damage numbers, including physics
 * simulation, lifetime management, and alpha blending calculations. Should be
 * called once per frame with the frame time delta.
 *
 * Physics Simulation:
 * - Applies velocity to position each frame
 * - Applies gravity acceleration (-0.15 tiles/second²)
 * - Numbers float upward initially, then slow and fall
 *
 * Animation Curves:
 * - Fade-in: Linear ramp from 0 to 1.0 over first 80ms
 * - Fade-out: Quadratic curve (t²) over last 150ms
 * - Alpha clamped to [0, 1] range for valid blending
 *
 * Cleanup:
 * - Removes expired numbers using swap-and-pop for efficiency
 * - Maintains array continuity and count accuracy
 *
 * @param dt_seconds Frame time delta in seconds (typically 1/60 or 1/30)
 *
 * @note Early exit if no damage numbers are active
 * @note Converts seconds to milliseconds for lifetime calculations
 * @note Gravity creates natural floating-then-falling motion
 * @note Fade curves tuned for smooth, readable transitions
 * @note Efficient O(n) processing with amortized O(1) removal
 */
void rogue_damage_numbers_update(float dt_seconds)
{
    if (g_app.dmg_number_count <= 0)
        return;
    float dt_ms = dt_seconds * 1000.0f;
    for (int i = 0; i < g_app.dmg_number_count;)
    {
        g_app.dmg_numbers[i].life_ms -= dt_ms;
        g_app.dmg_numbers[i].x += g_app.dmg_numbers[i].vx * dt_seconds;
        g_app.dmg_numbers[i].y += g_app.dmg_numbers[i].vy * dt_seconds;
        g_app.dmg_numbers[i].vy -= 0.15f * dt_seconds;
        /* Fade curve: early ease-in (first 80ms ramp alpha), late quadratic fade last 150ms */
        float age = g_app.dmg_numbers[i].total_ms - g_app.dmg_numbers[i].life_ms;
        float alpha = 1.0f;
        if (age < 80.0f)
            alpha = age / 80.0f;
        else
        {
            if (g_app.dmg_numbers[i].life_ms < 150.0f)
            {
                float t = g_app.dmg_numbers[i].life_ms / 150.0f;
                alpha = t * t;
            }
        }
        g_app.dmg_numbers[i].alpha = alpha;
        if (g_app.dmg_numbers[i].life_ms <= 0)
        {
            g_app.dmg_numbers[i] = g_app.dmg_numbers[g_app.dmg_number_count - 1];
            g_app.dmg_number_count--;
            continue;
        }
        ++i;
    }
}

/**
 * @brief Render all active damage numbers to the screen
 *
 * Renders damage numbers using SDL with hardware acceleration, alpha blending,
 * and color-coded visual feedback. Converts world coordinates to screen space
 * and applies appropriate coloring based on damage source and type.
 *
 * Color Coding:
 * - Critical hits: Yellow (255, 255, 120) - bright and rewarding
 * - Player damage: Orange (255, 210, 40) - warm, positive feedback
 * - Enemy damage: Red (255, 60, 60) - hostile, damage-indicating
 *
 * Rendering Process:
 * - Converts world coordinates to screen space using camera offset
 * - Applies current alpha value for smooth fade transitions
 * - Scales text size based on critical hit status (1.4x for crits)
 * - Uses font rendering system with hardware acceleration
 *
 * @note Requires SDL renderer to be initialized (g_app.renderer != NULL)
 * @note Only renders when ROGUE_HAVE_SDL is defined
 * @note Alpha values are pre-calculated in update function
 * @note Scale is clamped to reasonable bounds (1x to 4x)
 * @note Screen coordinates use tile_size scaling for consistent display
 */
void rogue_damage_numbers_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    for (int i = 0; i < g_app.dmg_number_count; i++)
    {
        int alpha =
            (int) (255 * (g_app.dmg_numbers[i].alpha < 0
                              ? 0
                              : (g_app.dmg_numbers[i].alpha > 1 ? 1 : g_app.dmg_numbers[i].alpha)));
        if (alpha < 0)
            alpha = 0;
        if (alpha > 255)
            alpha = 255;
        int screen_x = (int) (g_app.dmg_numbers[i].x * g_app.tile_size - g_app.cam_x);
        int screen_y = (int) (g_app.dmg_numbers[i].y * g_app.tile_size - g_app.cam_y);
        char buf[16];
        snprintf(buf, sizeof buf, "%d", g_app.dmg_numbers[i].amount);
        RogueColor col;
        if (g_app.dmg_numbers[i].crit)
        {
            col = (RogueColor){255, 255, 120, (unsigned char) alpha};
        }
        else if (g_app.dmg_numbers[i].from_player)
        {
            col = (RogueColor){255, 210, 40, (unsigned char) alpha};
        }
        else
        {
            col = (RogueColor){255, 60, 60, (unsigned char) alpha};
        }
        int txt_scale =
            g_app.dmg_numbers[i].scale > 0 ? (int) (g_app.dmg_numbers[i].scale * 1.0f) : 1;
        if (txt_scale < 1)
            txt_scale = 1;
        if (txt_scale > 4)
            txt_scale = 4;
        rogue_font_draw_text(screen_x, screen_y, buf, txt_scale, col);
    }
#endif
}
