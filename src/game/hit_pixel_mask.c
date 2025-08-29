/**
 * @file hit_pixel_mask.c
 * @brief Pixel-based hit detection system for precise weapon collision detection
 *
 * This module implements a pixel-perfect hit detection system for weapons in the roguelike game.
 * It provides efficient bit-packed pixel masks for accurate collision detection between weapon
 * attack frames and enemy positions, with support for rotation, scaling, and pose transformations.
 *
 * The system uses lazy loading to generate or load pixel masks for weapons on demand, falling
 * back to simple placeholder masks when asset loading is not yet implemented.
 */

#include "hit_pixel_mask.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/** @brief Global toggle for enabling pixel mask hit detection (default off until validated) */
int g_hit_use_pixel_masks = 0; /* default off until pixel path validated */

/** @brief Maximum number of pixel mask sets that can be cached simultaneously */
#define MAX_PIXEL_MASK_SETS 16

/** @brief Static array storing all loaded pixel mask sets */
static RogueHitPixelMaskSet g_sets[MAX_PIXEL_MASK_SETS];

/** @brief Current count of loaded pixel mask sets */
static int g_set_count = 0;

/**
 * @brief Finds an existing pixel mask set for the specified weapon ID
 *
 * Searches through the loaded mask sets to find one matching the given weapon ID.
 *
 * @param weapon_id The weapon identifier to search for
 * @return Pointer to the found mask set, or NULL if not found
 */
static RogueHitPixelMaskSet* find_set(int weapon_id)
{
    for (int i = 0; i < g_set_count; i++)
        if (g_sets[i].weapon_id == weapon_id)
            return &g_sets[i];
    return NULL;
}

/**
 * @brief Allocates and initializes a pixel mask frame with the specified dimensions
 *
 * Creates a bit-packed pixel mask frame with proper memory allocation and initialization.
 * The frame uses 32-bit words for efficient storage and fast bit operations.
 *
 * @param f Pointer to the frame structure to initialize
 * @param w Width of the frame in pixels
 * @param h Height of the frame in pixels
 */
static void alloc_frame(RogueHitPixelMaskFrame* f, int w, int h)
{
    if (!f)
        return;
    if (w <= 0 || h <= 0)
        return;
    f->width = w;
    f->height = h;
    f->origin_x = 0;
    f->origin_y = 0;
    f->pitch_words = (w + 31) / 32;
    size_t words = (size_t) f->pitch_words * (size_t) h;
    f->bits = (uint32_t*) malloc(words * sizeof(uint32_t));
    if (f->bits)
        memset(f->bits, 0, words * sizeof(uint32_t));
}

/**
 * @brief Ensures a pixel mask set exists for the specified weapon ID
 *
 * This function implements lazy loading for pixel mask sets. If a mask set for the weapon
 * already exists and is ready, it returns the cached set. Otherwise, it creates a new set
 * and generates placeholder mask data.
 *
 * The placeholder generation creates simple horizontal bar masks that simulate weapon
 * swing progression over 8 frames, with each frame showing slight forward advancement.
 *
 * @param weapon_id The weapon identifier for which to ensure mask availability
 * @return Pointer to the ready mask set, or NULL if allocation failed
 * @note Currently generates placeholder masks until proper asset loading is implemented
 */
RogueHitPixelMaskSet* rogue_hit_pixel_masks_ensure(int weapon_id)
{
    RogueHitPixelMaskSet* s = find_set(weapon_id);
    if (s && s->ready)
        return s;
    if (!s)
    {
        if (g_set_count >= MAX_PIXEL_MASK_SETS)
            return NULL;
        s = &g_sets[g_set_count++];
        memset(s, 0, sizeof *s);
        s->weapon_id = weapon_id;
    }
    /* Placeholder generation: simple horizontal bar mask representing weapon length */
    s->frame_count = 8;
    for (int i = 0; i < 8; i++)
    {
        alloc_frame(&s->frames[i], 48, 16); /* 48x16 area */
        /* Fill a simple line with thickness 4 pixels; simulate slight forward progression per frame
         */
        int advance = i * 4;
        if (advance > 24)
            advance = 24;
        for (int y = 6; y < 10; y++)
        {
            for (int x = advance; x < advance + 24; ++x)
            {
                rogue_hit_mask_set(&s->frames[i], x, y);
            }
        }
    }
    s->ready = 1;
    return s;
}

/**
 * @brief Releases all allocated pixel mask data and resets the system
 *
 * This function performs cleanup of all loaded pixel mask sets, freeing allocated
 * memory for individual frames and resetting the global state. Used primarily
 * for test teardown and memory management.
 *
 * @note This function should be called when shutting down the game or during
 *       testing to prevent memory leaks
 */
void rogue_hit_pixel_masks_reset_all(void)
{
    for (int i = 0; i < g_set_count; i++)
    {
        for (int f = 0; f < g_sets[i].frame_count; ++f)
        {
            free(g_sets[i].frames[f].bits);
            g_sets[i].frames[f].bits = NULL;
        }
    }
    g_set_count = 0;
}
/**
 * @brief Computes the axis-aligned bounding box of a pixel mask frame
 *
 * Returns the dimensions of the mask frame in local mask space, aligned to the origin.
 * This is useful for broad-phase collision detection before performing pixel-perfect tests.
 *
 * @param f Pointer to the pixel mask frame to query
 * @param out_w Pointer to store the width (can be NULL)
 * @param out_h Pointer to store the height (can be NULL)
 * @note Returns 0 for both dimensions if the frame pointer is NULL
 */
void rogue_hit_mask_frame_aabb(const RogueHitPixelMaskFrame* f, int* out_w, int* out_h)
{
    if (out_w)
        *out_w = f ? f->width : 0;
    if (out_h)
        *out_h = f ? f->height : 0;
}
/**
 * @brief Tests enemy collision against a pixel mask frame using multi-point sampling
 *
 * Performs pixel-perfect collision detection between an enemy (represented as a circle)
 * and a weapon attack frame. Uses a two-stage sampling approach:
 * 1. Tests the enemy center point
 * 2. If center misses, tests 8 points around the enemy perimeter at 70% radius
 *
 * This provides efficient yet accurate hit detection for circular enemy hitboxes.
 *
 * @param f Pointer to the pixel mask frame to test against
 * @param enemy_cx_local Enemy center X coordinate in local mask space
 * @param enemy_cy_local Enemy center Y coordinate in local mask space
 * @param enemy_radius Enemy collision radius
 * @param out_lx Pointer to store the local X coordinate of hit point (can be NULL)
 * @param out_ly Pointer to store the local Y coordinate of hit point (can be NULL)
 * @return 1 if enemy collides with mask, 0 otherwise
 * @note Returns 0 if frame is invalid or has no allocated bits
 */
int rogue_hit_mask_enemy_test(const RogueHitPixelMaskFrame* f, float enemy_cx_local,
                              float enemy_cy_local, float enemy_radius, int* out_lx, int* out_ly)
{
    if (!f || !f->bits)
        return 0; /* sample center first */
    int cx = (int) enemy_cx_local, cy = (int) enemy_cy_local;
    if (rogue_hit_mask_test(f, cx, cy))
    {
        if (out_lx)
            *out_lx = cx;
        if (out_ly)
            *out_ly = cy;
        return 1;
    } /* ring sample 8 points */
    float r = enemy_radius * 0.7f;
    for (int i = 0; i < 8; i++)
    {
        float ang = (float) (i * 3.14159265358979323846 * 0.25);
        float sx = enemy_cx_local + r * (float) cos(ang);
        float sy = enemy_cy_local + r * (float) sin(ang);
        int ix = (int) sx, iy = (int) sy;
        if (rogue_hit_mask_test(f, ix, iy))
        {
            if (out_lx)
                *out_lx = ix;
            if (out_ly)
                *out_ly = iy;
            return 1;
        }
    }
    return 0;
}
/**
 * @brief Converts local pixel coordinates to world space coordinates
 *
 * Transforms a pixel position from the local mask coordinate system to world space,
 * accounting for player position, pose offsets, scaling, and rotation. This is essential
 * for accurate hit positioning and visual effects placement.
 *
 * The transformation applies the following steps:
 * 1. Convert pixel coordinates to local space relative to mask origin
 * 2. Apply scaling
 * 3. Apply rotation around the origin
 * 4. Add player position and pose offsets
 *
 * @param f Pointer to the pixel mask frame (used for origin offset, can be NULL)
 * @param lx Local X coordinate in mask space
 * @param ly Local Y coordinate in mask space
 * @param player_x Player's world X position
 * @param player_y Player's world Y position
 * @param pose_dx Additional X offset from player pose
 * @param pose_dy Additional Y offset from player pose
 * @param scale Uniform scaling factor to apply
 * @param angle_rad Rotation angle in radians (counter-clockwise)
 * @param out_wx Pointer to store world X coordinate (can be NULL)
 * @param out_wy Pointer to store world Y coordinate (can be NULL)
 * @note If frame is NULL, returns player position without transformation
 */
void rogue_hit_mask_local_pixel_to_world(const RogueHitPixelMaskFrame* f, int lx, int ly,
                                         float player_x, float player_y, float pose_dx,
                                         float pose_dy, float scale, float angle_rad, float* out_wx,
                                         float* out_wy)
{
    if (!f)
    {
        if (out_wx)
            *out_wx = player_x;
        if (out_wy)
            *out_wy = player_y;
        return;
    }
    float x = (float) (lx - f->origin_x + 0.5f) * scale;
    float y = (float) (ly - f->origin_y + 0.5f) * scale;
    float ca = (float) cos(angle_rad), sa = (float) sin(angle_rad);
    float rx = x * ca - y * sa;
    float ry = x * sa + y * ca;
    if (out_wx)
        *out_wx = player_x + pose_dx + rx;
    if (out_wy)
        *out_wy = player_y + pose_dy + ry;
}
