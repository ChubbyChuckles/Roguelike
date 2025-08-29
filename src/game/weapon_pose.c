/**
 * @file weapon_pose.c
 * @brief Weapon pose and animation system for roguelike game.
 *
 * This module provides a comprehensive weapon pose system supporting both single-direction
 * and multi-directional weapon animations. It handles loading weapon pose data from JSON
 * configuration files and provides texture management for SDL rendering.
 *
 * The system supports:
 * - Single-direction weapon poses (8 frames per weapon)
 * - Multi-directional weapon poses (3 directions × 8 frames per weapon)
 * - JSON-based pose configuration with transform parameters
 * - SDL texture loading and caching
 * - Fallback to default poses when configuration files are missing
 *
 * @note Pose data includes position offsets (dx, dy), rotation angle, scale, and pivot points
 * @note Textures are loaded from BMP files in the assets/weapons directory
 * @note System gracefully degrades when SDL is not available
 */

#include "weapon_pose.h"
#include "../core/app/app_state.h"
#include "../util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/** Maximum number of weapon pose sets that can be loaded simultaneously */
#define ROGUE_WEAPON_POSE_MAX 32
/** Number of animation frames per weapon pose (0-7) */
#define FRAME_COUNT 8

/**
 * @brief Single-direction weapon pose set structure.
 *
 * Contains pose data and textures for weapons that use a single animation sequence
 * regardless of facing direction.
 */
typedef struct WeaponPoseSet
{
    int weapon_id;                            /**< Unique weapon identifier */
    int loaded;                               /**< Flag indicating if pose data has been loaded */
    RogueWeaponPoseFrame frames[FRAME_COUNT]; /**< Array of pose frames with transform data */
#ifdef ROGUE_HAVE_SDL
    SDL_Texture* textures[FRAME_COUNT]; /**< SDL textures for each animation frame */
    int tw[FRAME_COUNT];                /**< Texture width for each frame */
    int th[FRAME_COUNT];                /**< Texture height for each frame */
#endif
} WeaponPoseSet;

/** Global array of single-direction weapon pose sets */
static WeaponPoseSet g_pose_sets[ROGUE_WEAPON_POSE_MAX];
/** Current count of loaded single-direction pose sets */
static int g_pose_count = 0;

/**
 * @brief Multi-directional weapon pose set structure.
 *
 * Supports weapons with different animations based on facing direction (down, up, side).
 * Uses a single shared texture for all directions to optimize memory usage.
 */
typedef struct WeaponPoseDirSet
{
    int weapon_id;     /**< Unique weapon identifier */
    int loaded_dir[3]; /**< Load status per direction (0=down, 1=up, 2=side) */
    RogueWeaponPoseFrame frames[3][FRAME_COUNT]; /**< Pose frames for each direction */
#ifdef ROGUE_HAVE_SDL
    SDL_Texture* texture_single; /**< Single shared texture for all directions */
    int tex_w, tex_h;            /**< Shared texture dimensions */
#endif
} WeaponPoseDirSet;

/** Global array of multi-directional weapon pose sets */
static WeaponPoseDirSet g_dir_sets[ROGUE_WEAPON_POSE_MAX];
/** Current count of loaded multi-directional pose sets */
static int g_dir_count = 0;

/**
 * @brief Get or create a directional pose set for the specified weapon.
 *
 * Searches for an existing directional pose set for the weapon ID. If not found,
 * creates a new one if capacity allows.
 *
 * @param wid Weapon ID to retrieve pose set for
 * @return Pointer to the directional pose set, or NULL if capacity exceeded
 *
 * @note Initializes loaded_dir array to zeros for new pose sets
 * @note Thread-unsafe - should only be called from main thread
 */
static WeaponPoseDirSet* get_dir_set(int wid)
{
    for (int i = 0; i < g_dir_count; i++)
        if (g_dir_sets[i].weapon_id == wid)
            return &g_dir_sets[i];
    if (g_dir_count < ROGUE_WEAPON_POSE_MAX)
    {
        g_dir_sets[g_dir_count].weapon_id = wid;
        memset(g_dir_sets[g_dir_count].loaded_dir, 0, sizeof(g_dir_sets[g_dir_count].loaded_dir));
        return &g_dir_sets[g_dir_count++];
    }
    return NULL;
}

/**
 * @brief Get or create a single-direction pose set for the specified weapon.
 *
 * Searches for an existing single-direction pose set for the weapon ID. If not found,
 * creates a new one if capacity allows.
 *
 * @param weapon_id Weapon ID to retrieve pose set for
 * @return Pointer to the pose set, or NULL if capacity exceeded
 *
 * @note Thread-unsafe - should only be called from main thread
 */
static WeaponPoseSet* get_set(int weapon_id)
{
    for (int i = 0; i < g_pose_count; i++)
        if (g_pose_sets[i].weapon_id == weapon_id)
            return &g_pose_sets[i];
    if (g_pose_count < ROGUE_WEAPON_POSE_MAX)
    {
        g_pose_sets[g_pose_count].weapon_id = weapon_id;
        return &g_pose_sets[g_pose_count++];
    }
    return NULL;
}

/**
 * @brief Parse a float value from a string.
 *
 * Safely converts a string representation to a float value using strtod.
 * Validates that the conversion was successful.
 *
 * @param s Input string to parse
 * @param out Pointer to store the parsed float value
 * @return 1 on successful parsing, 0 on failure
 *
 * @note Uses strtod for robust float parsing
 * @note Validates that at least one character was consumed during parsing
 */
static int parse_float(const char* s, float* out)
{
    char* e = NULL;
    float v = (float) strtod(s, &e);
    if (e == s)
        return 0;
    *out = v;
    return 1;
}

/**
 * @brief Initialize weapon pose frames with default values.
 *
 * Sets all pose frames to neutral/default transform values suitable for
 * basic weapon rendering without pose data.
 *
 * @param set Pointer to the weapon pose set to initialize
 *
 * @note Default values: dx=0, dy=0, angle=0, scale=1.0, pivot=(0.5, 0.5)
 * @note Called when JSON loading fails or for fallback poses
 */
static void default_frames(WeaponPoseSet* set)
{
    for (int i = 0; i < FRAME_COUNT; i++)
    {
        set->frames[i].dx = 0;
        set->frames[i].dy = 0;
        set->frames[i].angle = 0;
        set->frames[i].scale = 1.0f;
        set->frames[i].pivot_x = 0.5f;
        set->frames[i].pivot_y = 0.5f;
    }
}

/**
 * @brief Initialize directional weapon pose frames with default values.
 *
 * Sets pose frames for a specific direction to neutral/default transform values.
 *
 * @param frames Array of frames to initialize (FRAME_COUNT elements)
 *
 * @note Default values: dx=0, dy=0, angle=0, scale=1.0, pivot=(0.5, 0.5)
 * @note Used for directional pose fallback when JSON loading fails
 */
static void default_frames_dir(RogueWeaponPoseFrame* frames)
{
    for (int i = 0; i < FRAME_COUNT; i++)
    {
        frames[i].dx = 0;
        frames[i].dy = 0;
        frames[i].angle = 0;
        frames[i].scale = 1.0f;
        frames[i].pivot_x = 0.5f;
        frames[i].pivot_y = 0.5f;
    }
}

/**
 * @brief Load weapon pose data from JSON configuration file.
 *
 * Parses a JSON file containing weapon pose animation data for a single-direction weapon.
 * The JSON format contains an array of frame objects with transform parameters.
 *
 * @param weapon_id Weapon ID to load pose data for
 * @param set Pointer to weapon pose set to populate
 * @return 1 on successful loading, 0 on failure (falls back to defaults)
 *
 * @note JSON file format: weapon_[id]_pose.json in assets/weapons/
 * @note Supports multiple path attempts (../assets/, ../../assets/)
 * @note Falls back to default frames if JSON parsing fails
 * @note File size limited to 32KB for security
 * @note Parses frame array with dx, dy, angle, scale, pivot_x, pivot_y properties
 */
static int load_json_pose(int weapon_id, WeaponPoseSet* set)
{
    char path[256];
    char path_alt[256];
    snprintf(path, sizeof path, "../assets/weapons/weapon_%d_pose.json", weapon_id);
    snprintf(path_alt, sizeof path_alt, "../../assets/weapons/weapon_%d_pose.json", weapon_id);
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        /* try alternate relative root */
#if defined(_MSC_VER)
        fopen_s(&f, path_alt, "rb");
#else
        f = fopen(path_alt, "rb");
#endif
        if (!f)
        {
            ROGUE_LOG_DEBUG("weapon_pose_json_open_fail: %s | %s", path, path_alt);
            default_frames(set);
            return 0;
        }
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 32768)
    {
        fclose(f);
        default_frames(set);
        return 0;
    }
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        default_frames(set);
        return 0;
    }
    if (fread(buf, 1, (size_t) sz, f) != (size_t) sz)
    {
        free(buf);
        fclose(f);
        default_frames(set);
        return 0;
    }
    buf[sz] = '\0';
    fclose(f);
    default_frames(set);
    const char* s = buf;
    int frame_idx = 0;
    while (*s && frame_idx < FRAME_COUNT)
    {
        const char* fkey = strstr(s, "\"frames\"");
        if (!fkey)
            break;
        s = fkey;
        while (*s && *s != '[')
            s++;
        if (*s != '[')
            break;
        s++; /* inside frames */
        while (*s && frame_idx < FRAME_COUNT)
        {
            while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t' || *s == ','))
                s++;
            if (*s == ']' || !*s)
                break;
            if (*s != '{')
            {
                s++;
                continue;
            }
            s++;
            RogueWeaponPoseFrame fr = set->frames[frame_idx];
            while (*s && *s != '}')
            {
                while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t' || *s == ','))
                    s++;
                if (*s != '"')
                {
                    s++;
                    continue;
                }
                s++;
                const char* key = s;
                while (*s && *s != '"')
                    s++;
                if (!*s)
                    break;
                size_t klen = (size_t) (s - key);
                s++;
                while (*s && *s != ':')
                    s++;
                if (*s == ':')
                    s++;
                while (*s == ' ' || *s == '\n' || *s == '\r')
                    s++;
                char val[64];
                int vi = 0;
                if (*s == '"')
                {
                    s++;
                    while (*s && *s != '"' && vi < 63)
                    {
                        val[vi++] = *s++;
                    }
                    if (*s == '"')
                        s++;
                }
                else
                {
                    while (*s && *s != ',' && *s != '}' && vi < 63)
                    {
                        val[vi++] = *s++;
                    }
                }
                val[vi] = '\0';
                float fv;
                if (parse_float(val, &fv))
                {
                    if (klen == 2 && strncmp(key, "dx", 2) == 0)
                        fr.dx = fv;
                    else if (klen == 2 && strncmp(key, "dy", 2) == 0)
                        fr.dy = fv;
                    else if (klen == 5 && strncmp(key, "angle", 5) == 0)
                        fr.angle = fv;
                    else if (klen == 5 && strncmp(key, "scale", 5) == 0)
                        fr.scale = fv;
                    else if (klen == 7 && strncmp(key, "pivot_x", 7) == 0)
                        fr.pivot_x = fv;
                    else if (klen == 7 && strncmp(key, "pivot_y", 7) == 0)
                        fr.pivot_y = fv;
                }
            }
            while (*s && *s != '}')
                s++;
            if (*s == '}')
            {
                s++;
                set->frames[frame_idx] = fr;
                frame_idx++;
            }
        }
    }
    free(buf);
    return 1;
}

/**
 * @brief Load directional weapon pose data from JSON configuration file.
 *
 * Parses a JSON file containing weapon pose animation data for a specific direction.
 * Falls back to generic pose data or defaults if directional JSON is not available.
 *
 * @param weapon_id Weapon ID to load pose data for
 * @param dir_group Direction group (0=down, 1=up, 2=side)
 * @param out_frames Array to store parsed frame data (FRAME_COUNT elements)
 * @return 1 on successful loading or fallback, 0 on complete failure
 *
 * @note JSON file format: weapon_[id]_[direction]_pose.json
 * @note Direction suffixes: "down", "up", "side"
 * @note Falls back to generic pose if directional file missing
 * @note Falls back to defaults if both directional and generic poses missing
 * @note Always returns success (1) as defaults are applied on any failure
 */
static int load_json_pose_dir(int weapon_id, int dir_group, RogueWeaponPoseFrame* out_frames)
{
    /* dir suffix mapping */
    const char* suffix = (dir_group == 0) ? "down" : (dir_group == 1 ? "up" : "side");
    char path[256];
    char path_alt[256];
    snprintf(path, sizeof path, "../assets/weapons/weapon_%d_%s_pose.json", weapon_id, suffix);
    snprintf(path_alt, sizeof path_alt, "../../assets/weapons/weapon_%d_%s_pose.json", weapon_id,
             suffix);
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        /* Try alternate relative root first */
#if defined(_MSC_VER)
        fopen_s(&f, path_alt, "rb");
#else
        f = fopen(path_alt, "rb");
#endif
        if (!f)
        {
            /* If the directional pose JSON is missing, fall back to the generic pose
               (weapon_%d_pose.json). If that is also missing, use defaults. */
            ROGUE_LOG_DEBUG("weapon_pose_dir_json_open_fail: %s", path);
            default_frames_dir(out_frames);
            /* attempt to populate from generic pose if available */
            if (rogue_weapon_pose_ensure(weapon_id))
            {
                for (int i = 0; i < FRAME_COUNT; i++)
                {
                    const RogueWeaponPoseFrame* base = rogue_weapon_pose_get(weapon_id, i);
                    if (base)
                        out_frames[i] = *base;
                }
            }
            return 1;
        }
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 32768)
    {
        fclose(f);
        default_frames_dir(out_frames);
        return 1;
    }
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        default_frames_dir(out_frames);
        return 1;
    }
    if (fread(buf, 1, (size_t) sz, f) != (size_t) sz)
    {
        free(buf);
        fclose(f);
        default_frames_dir(out_frames);
        return 1;
    }
    buf[sz] = '\0';
    fclose(f);
    default_frames_dir(out_frames);
    const char* s = buf;
    int frame_idx = 0;
    const char* fkey = strstr(s, "\"frames\"");
    if (fkey)
    {
        s = fkey;
        while (*s && *s != '[')
            s++;
        if (*s == '[')
        {
            s++;
            while (*s && frame_idx < FRAME_COUNT)
            {
                while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t' || *s == ','))
                    s++;
                if (*s == ']' || !*s)
                    break;
                if (*s != '{')
                {
                    s++;
                    continue;
                }
                s++;
                RogueWeaponPoseFrame fr = out_frames[frame_idx];
                while (*s && *s != '}')
                {
                    while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t' || *s == ','))
                        s++;
                    if (*s != '\"')
                    {
                        s++;
                        continue;
                    }
                    s++;
                    const char* key = s;
                    while (*s && *s != '\"')
                        s++;
                    if (!*s)
                        break;
                    size_t klen = (size_t) (s - key);
                    s++;
                    while (*s && *s != ':')
                        s++;
                    if (*s == ':')
                        s++;
                    while (*s == ' ' || *s == '\n' || *s == '\r')
                        s++;
                    char val[64];
                    int vi = 0;
                    if (*s == '\"')
                    {
                        s++;
                        while (*s && *s != '\"' && vi < 63)
                        {
                            val[vi++] = *s++;
                        }
                        if (*s == '\"')
                            s++;
                    }
                    else
                    {
                        while (*s && *s != ',' && *s != '}' && vi < 63)
                        {
                            val[vi++] = *s++;
                        }
                    }
                    val[vi] = '\0';
                    float fv;
                    if (parse_float(val, &fv))
                    {
                        if (klen == 2 && strncmp(key, "dx", 2) == 0)
                            fr.dx = fv;
                        else if (klen == 2 && strncmp(key, "dy", 2) == 0)
                            fr.dy = fv;
                        else if (klen == 5 && strncmp(key, "angle", 5) == 0)
                            fr.angle = fv;
                        else if (klen == 5 && strncmp(key, "scale", 5) == 0)
                            fr.scale = fv;
                        else if (klen == 7 && strncmp(key, "pivot_x", 7) == 0)
                            fr.pivot_x = fv;
                        else if (klen == 7 && strncmp(key, "pivot_y", 7) == 0)
                            fr.pivot_y = fv;
                    }
                }
                while (*s && *s != '}')
                    s++;
                if (*s == '}')
                {
                    s++;
                    out_frames[frame_idx] = fr;
                    frame_idx++;
                }
            }
        }
    }
    free(buf);
    ROGUE_LOG_DEBUG("weapon_pose_dir_loaded: wid=%d dir=%d frames=%d", weapon_id, dir_group,
                    frame_idx);
    /* Consider fallback successful even if no frames were parsed from file, as
        defaults were applied above. */
    return frame_idx >= 0;
}

/**
 * @brief Ensure directional weapon pose data is loaded for the specified weapon and direction.
 *
 * Loads directional pose data if not already loaded. Creates pose set if it doesn't exist.
 *
 * @param weapon_id Weapon ID to ensure pose data for
 * @param dir_group Direction group (0=down, 1=up, 2=side)
 * @return 1 on success, 0 on failure (invalid direction or capacity exceeded)
 *
 * @note Creates directional pose set if it doesn't exist
 * @note Only loads data once per weapon-direction combination
 * @note Invalid direction groups (outside 0-2) return failure
 */
int rogue_weapon_pose_ensure_dir(int weapon_id, int dir_group)
{
    WeaponPoseDirSet* ds = get_dir_set(weapon_id);
    if (!ds)
        return 0;
    if (dir_group < 0 || dir_group > 2)
        return 0;
    if (ds->loaded_dir[dir_group])
        return 1;
    if (load_json_pose_dir(weapon_id, dir_group, ds->frames[dir_group]))
        ds->loaded_dir[dir_group] = 1;
    else
        ds->loaded_dir[dir_group] = 0;
    return ds->loaded_dir[dir_group];
}

/**
 * @brief Get directional weapon pose frame data.
 *
 * Retrieves pose frame data for a specific weapon, direction, and animation frame.
 *
 * @param weapon_id Weapon ID to get pose data for
 * @param dir_group Direction group (0=down, 1=up, 2=side)
 * @param frame_index Animation frame index (0-7)
 * @return Pointer to pose frame data, or NULL if invalid parameters or not loaded
 *
 * @note Returns NULL for invalid direction groups or frame indices
 * @note Returns NULL if pose data hasn't been loaded for the weapon-direction
 */
const RogueWeaponPoseFrame* rogue_weapon_pose_get_dir(int weapon_id, int dir_group, int frame_index)
{
    WeaponPoseDirSet* ds = get_dir_set(weapon_id);
    if (!ds || dir_group < 0 || dir_group > 2 || !ds->loaded_dir[dir_group] || frame_index < 0 ||
        frame_index >= FRAME_COUNT)
        return NULL;
    return &ds->frames[dir_group][frame_index];
}

/**
 * @brief Get single shared texture for directional weapon poses.
 *
 * Loads and returns the shared texture used for all directions of a directional weapon.
 * The texture is loaded from weapon_[id].bmp in the assets directory.
 *
 * @param weapon_id Weapon ID to get texture for
 * @param w Pointer to store texture width (can be NULL)
 * @param h Pointer to store texture height (can be NULL)
 * @return Pointer to SDL texture, or NULL if loading failed or SDL not available
 *
 * @note Texture is loaded once and cached for the weapon
 * @note Only available when ROGUE_HAVE_SDL is defined
 * @note Returns NULL when SDL is not available (graceful degradation)
 * @note Texture path: ../assets/weapons/weapon_[id].bmp
 */
void* rogue_weapon_pose_get_texture_single(int weapon_id, int* w, int* h)
{
#ifdef ROGUE_HAVE_SDL
    WeaponPoseDirSet* ds = get_dir_set(weapon_id);
    if (!ds)
        return NULL;
    if (!ds->texture_single)
    {
        /* load a shared weapon bitmap (frame0) using ../assets path */
        char path[256];
        snprintf(path, sizeof path, "../assets/weapons/weapon_%d.bmp", weapon_id);
        SDL_Surface* surf = SDL_LoadBMP(path);
        if (!surf)
        {
            ROGUE_LOG_DEBUG("weapon_pose_dir_tex_open_fail: %s", path);
            return NULL;
        }
        ds->texture_single = SDL_CreateTextureFromSurface(g_app.renderer, surf);
        ds->tex_w = surf->w;
        ds->tex_h = surf->h;
        SDL_FreeSurface(surf);
        ROGUE_LOG_DEBUG("weapon_pose_dir_tex_loaded: wid=%d %dx%d", weapon_id, ds->tex_w,
                        ds->tex_h);
    }
    if (w)
        *w = ds->tex_w;
    if (h)
        *h = ds->tex_h;
    return ds->texture_single;
#else
    (void) weapon_id;
    (void) w;
    (void) h;
    return NULL;
#endif
}

/**
 * @brief Load SDL textures for weapon animation frames.
 *
 * Loads individual BMP textures for each animation frame of a single-direction weapon.
 * Textures are loaded from weapon_[id]_f[frame].bmp files.
 *
 * @param weapon_id Weapon ID to load textures for
 * @param set Pointer to weapon pose set containing texture pointers
 * @param renderer SDL renderer to create textures with
 *
 * @note Only available when ROGUE_HAVE_SDL is defined
 * @note Texture path format: ../assets/weapons/weapon_[id]_f[frame].bmp
 * @note Falls back to previous frame's texture if current frame fails to load
 * @note Stores texture dimensions in set->tw and set->th arrays
 */
#ifdef ROGUE_HAVE_SDL
static void load_textures(int weapon_id, WeaponPoseSet* set, SDL_Renderer* renderer)
{
    for (int i = 0; i < FRAME_COUNT; i++)
    {
        char path[256];
        snprintf(path, sizeof path, "../assets/weapons/weapon_%d_f%d.bmp", weapon_id, i);
        SDL_Surface* surf = SDL_LoadBMP(path);
        if (!surf)
        {
            if (i > 0)
            {
                set->textures[i] = set->textures[i - 1];
                set->tw[i] = set->tw[i - 1];
                set->th[i] = set->th[i - 1];
                continue;
            }
        }
        if (surf)
        {
            set->textures[i] = SDL_CreateTextureFromSurface(renderer, surf);
            set->tw[i] = surf->w;
            set->th[i] = surf->h;
            SDL_FreeSurface(surf);
        }
    }
}
#endif

/**
 * @brief Ensure single-direction weapon pose data is loaded.
 *
 * Loads weapon pose data and textures if not already loaded. Creates pose set if needed.
 *
 * @param weapon_id Weapon ID to ensure pose data for
 * @return 1 on success, 0 on failure (capacity exceeded)
 *
 * @note Creates pose set if it doesn't exist
 * @note Loads JSON pose data and SDL textures (if available)
 * @note Only loads data once per weapon ID
 * @note Falls back to default frames if JSON loading fails
 */
int rogue_weapon_pose_ensure(int weapon_id)
{
    WeaponPoseSet* set = get_set(weapon_id);
    if (!set)
        return 0;
    if (set->loaded)
        return 1;
    if (!load_json_pose(weapon_id, set))
    {
        ROGUE_LOG_DEBUG("weapon_pose_default_used: %d", weapon_id);
    }
#ifdef ROGUE_HAVE_SDL
    extern struct RogueAppState g_app;
    if (g_app.renderer)
        load_textures(weapon_id, set, g_app.renderer);
#endif
    set->loaded = 1;
    return 1;
}

/**
 * @brief Get single-direction weapon pose frame data.
 *
 * Retrieves pose frame data for a specific weapon and animation frame.
 *
 * @param weapon_id Weapon ID to get pose data for
 * @param frame_index Animation frame index (0-7)
 * @return Pointer to pose frame data, or NULL if invalid parameters or not loaded
 *
 * @note Returns NULL for invalid frame indices or unloaded weapons
 * @note Frame index must be in range 0 to FRAME_COUNT-1
 */
const RogueWeaponPoseFrame* rogue_weapon_pose_get(int weapon_id, int frame_index)
{
    WeaponPoseSet* set = get_set(weapon_id);
    if (!set || !set->loaded || frame_index < 0 || frame_index >= FRAME_COUNT)
        return NULL;
    return &set->frames[frame_index];
}

/**
 * @brief Get SDL texture for weapon animation frame.
 *
 * Retrieves the SDL texture for a specific weapon animation frame.
 *
 * @param weapon_id Weapon ID to get texture for
 * @param frame_index Animation frame index (0-7)
 * @param w Pointer to store texture width (can be NULL)
 * @param h Pointer to store texture height (can be NULL)
 * @return Pointer to SDL texture, or NULL if invalid parameters or SDL not available
 *
 * @note Only available when ROGUE_HAVE_SDL is defined
 * @note Returns NULL when SDL is not available (graceful degradation)
 * @note Returns NULL for invalid frame indices or unloaded weapons
 */
void* rogue_weapon_pose_get_texture(int weapon_id, int frame_index, int* w, int* h)
{
#ifdef ROGUE_HAVE_SDL
    WeaponPoseSet* set = get_set(weapon_id);
    if (!set || !set->loaded || frame_index < 0 || frame_index >= FRAME_COUNT)
        return NULL;
    if (w)
        *w = set->tw[frame_index];
    if (h)
        *h = set->th[frame_index];
    return (void*) set->textures[frame_index];
#else
    (void) weapon_id;
    (void) frame_index;
    (void) w;
    (void) h;
    return NULL;
#endif
}
