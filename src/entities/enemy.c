/**
 * @file enemy.c
 * @brief Implementation of enemy entity management system for the roguelike game.
 *
 * This module provides comprehensive enemy entity functionality including:
 * - JSON-based enemy type definitions with configurable stats and animations
 * - Directory-based loading of enemy configurations
 * - Legacy text-based configuration file support
 * - Sprite sheet loading and animation frame management
 * - Behavior tree AI integration framework
 * - Cross-platform file system operations
 *
 * The system supports multiple enemy types with varying difficulty tiers,
 * AI behaviors, and visual representations through sprite animations.
 *
 * @author Rogue Game Development Team
 * @date 2025
 * @version 1.0
 *
 * @note This implementation is part of the enemy integration phase of the roguelike game
 * development.
 * @see enemy.h for public API declarations
 */

#include "enemy.h"
#include "../util/log.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global flag to allow tests/headless validation to skip texture/sprite loading */
int g_enemy_loader_skip_textures = 0;

/* --- JSON Directory Loader (Phase: Enemy Integration) --- */
/**
 * @brief Reads an entire file into memory with size limits.
 *
 * Loads a complete file into a dynamically allocated buffer with safety checks
 * including file size limits and proper error handling. Supports both Windows
 * and Unix file APIs for cross-platform compatibility.
 *
 * @param path Path to the file to read
 * @param out_buf Output pointer for the allocated buffer (caller must free)
 * @param out_len Output pointer for the file size in bytes (optional)
 * @return 1 on success, 0 on failure
 *
 * @note Maximum file size is limited to 256KB for safety
 * @note Caller is responsible for freeing the returned buffer
 * @note Uses binary mode to preserve exact file contents
 */
static int read_entire_file(const char* path, char** out_buf, size_t* out_len)
{
    FILE* f = NULL;
    *out_buf = NULL;
    if (out_len)
        *out_len = 0;
#if defined(_MSC_VER)
    int fopen_result = fopen_s(&f, path, "rb");
    if (fopen_result != 0)
        f = NULL;
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return 0;
    }
    long sz = ftell(f);
    if (sz < 0 || sz > 262144)
    {
        fclose(f);
        return 0;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return 0;
    }
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        return 0;
    }
    size_t rd = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    if (rd != (size_t) sz)
    {
        free(buf);
        return 0;
    }
    buf[sz] = '\0';
    *out_buf = buf;
    if (out_len)
        *out_len = (size_t) sz;
    return 1;
}

/**
 * @brief Extracts a string value from JSON by key name.
 *
 * Searches for a JSON key-value pair and extracts the string value.
 * Performs basic JSON parsing to locate the key and extract the quoted string value.
 * Handles whitespace and validates JSON syntax minimally.
 *
 * @param json Pointer to the JSON string to parse
 * @param key The key name to search for (without quotes)
 * @param out Output buffer for the extracted string value
 * @param out_sz Size of the output buffer
 * @return 1 if key found and string extracted, 0 otherwise
 *
 * @note Simple parser - expects basic JSON format with quoted strings
 * @note Does not handle nested objects or complex JSON structures
 * @note Output is null-terminated and truncated if buffer too small
 */
static int json_find_string(const char* json, const char* key, char* out, size_t out_sz)
{
    if (!json || !key || !out || out_sz == 0)
        return 0;
    out[0] = '\0';
    const char* p = json;
    size_t klen = strlen(key);
    for (;;)
    {
        const char* found = strstr(p, key);
        if (!found)
            break;
        p = found; /* ensure it's a key: preceding char either { , or newline */
        const char* q = p + klen;
        while (*q && isspace((unsigned char) *q))
            q++;
        if (*q != ':')
        {
            p = p + 1;
            continue;
        }
        q++;
        while (*q && isspace((unsigned char) *q))
            q++;
        if (*q != '"')
        {
            p = p + 1;
            continue;
        }
        q++;
        const char* start = q;
        while (*q && *q != '"' && (q - start) < (ptrdiff_t) (out_sz - 1))
            q++;
        size_t len = (size_t) (q - start);
        if (*q == '"')
        {
            memcpy(out, start, len);
            out[len] = '\0';
            return 1;
        }
        p = p + 1;
    }
    return 0;
}

/**
 * @brief Extracts an integer value from JSON by key name.
 *
 * Searches for a JSON key-value pair and extracts the integer value.
 * Parses numeric values from JSON, handling negative numbers and basic integer formats.
 * Validates that the value is numeric and within integer range.
 *
 * @param json Pointer to the JSON string to parse
 * @param key The key name to search for (without quotes)
 * @param out Pointer to store the extracted integer value
 * @return 1 if key found and integer extracted, 0 otherwise
 *
 * @note Simple parser - expects basic JSON format with numeric values
 * @note Does not handle floating point or scientific notation
 * @note Validates numeric format but does not check integer overflow
 * @note Uses manual parsing instead of strtol for better control
 */
static int json_find_int(const char* json, const char* key, int* out)
{
    if (!json || !key || !out)
        return 0;
    const char* p = json;
    size_t klen = strlen(key);
    for (;;)
    {
        const char* found = strstr(p, key);
        if (!found)
            break;
        p = found;
        const char* q = p + klen;
        while (*q && isspace((unsigned char) *q))
            q++;
        if (*q != ':')
        {
            p = p + 1;
            continue;
        }
        q++;
        while (*q && isspace((unsigned char) *q))
            q++;
        char num[32];
        int ni = 0;
        int neg = 0;
        if (*q == '-')
        {
            neg = 1;
            q++;
        }
        while (*q && ni < 31 && (isdigit((unsigned char) *q)))
        {
            num[ni++] = *q++;
        }
        num[ni] = '\0';
        if (ni > 0)
        {
            int v = atoi(num);
            if (neg)
                v = -v;
            *out = v;
            return 1;
        }
        p = p + 1;
    }
    return 0;
}

/**
 * @brief Extracts a floating-point value from JSON by key name.
 *
 * Searches for a JSON key-value pair and extracts the float value.
 * Parses numeric values from JSON, handling negative numbers and decimal points.
 * Validates that the value is numeric and within float range.
 *
 * @param json Pointer to the JSON string to parse
 * @param key The key name to search for (without quotes)
 * @param out Pointer to store the extracted float value
 * @return 1 if key found and float extracted, 0 otherwise
 *
 * @note Simple parser - expects basic JSON format with numeric values
 * @note Handles decimal points but not scientific notation
 * @note Uses atof for conversion which may have precision limitations
 * @note Validates numeric format but does not check float overflow/underflow
 */
static int json_find_float(const char* json, const char* key, float* out)
{
    if (!json || !key || !out)
        return 0;
    const char* p = json;
    size_t klen = strlen(key);
    for (;;)
    {
        const char* found = strstr(p, key);
        if (!found)
            break;
        p = found;
        const char* q = p + klen;
        while (*q && isspace((unsigned char) *q))
            q++;
        if (*q != ':')
        {
            p = p + 1;
            continue;
        }
        q++;
        while (*q && isspace((unsigned char) *q))
            q++;
        char num[32];
        int ni = 0;
        int neg = 0;
        int dot = 0;
        if (*q == '-')
        {
            neg = 1;
            q++;
        }
        while (*q && ni < 31 && (isdigit((unsigned char) *q) || (!dot && *q == '.')))
        {
            if (*q == '.')
                dot = 1;
            num[ni++] = *q++;
        }
        num[ni] = '\0';
        if (ni > 0)
        {
            float v = (float) atof(num);
            if (neg)
                v = -v;
            *out = v;
            return 1;
        }
        p = p + 1;
    }
    return 0;
}

static int load_sheet(const char* path, RogueTexture* tex, RogueSprite frames[],
                      int* out_count); /* forward */
/**
 * @brief Loads enemy type definition from a JSON file.
 *
 * Parses a JSON file containing enemy configuration data and populates a RogueEnemyTypeDef
 * structure. Extracts all enemy properties including stats, behavior parameters, and sprite sheet
 * paths. Sets sensible defaults for missing optional fields.
 *
 * @param path Path to the JSON configuration file
 * @param out Pointer to RogueEnemyTypeDef structure to populate
 * @return 1 if file loaded and parsed successfully, 0 on failure
 *
 * @note Sets default values for missing fields (group_min=1, speed=30.0f, etc.)
 * @note Derives enemy name from ID if name field is missing
 * @note Loads associated sprite sheets for idle, run, and death animations
 * @note Validates group_max >= group_min constraint
 */
static int load_enemy_json_file(const char* path, RogueEnemyTypeDef* out)
{
    char* buf = NULL;
    size_t len = 0;
    if (!read_entire_file(path, &buf, &len))
        return 0;
    if (!buf)
        return 0;
    memset(out, 0, sizeof *out); /* defaults */
    out->group_min = 1;
    out->group_max = 1;
    out->patrol_radius = 4;
    out->aggro_radius = 5;
    out->speed = 30.0f;
    out->pop_target = 10;
    out->xp_reward = 1;
    out->loot_chance = 0.05f;
    out->base_level_offset = 0;
    out->tier_id = 0;
    out->archetype_id = 0;
    if (json_find_string(buf, "\"id\"", out->id, sizeof out->id) == 0)
    { /* derive id from filename later if empty */
        out->id[0] = '\0';
    }
    if (json_find_string(buf, "\"name\"", out->name, sizeof out->name) == 0)
    {
        if (out->id[0])
        {
#if defined(_MSC_VER)
            strncpy_s(out->name, sizeof out->name, out->id, _TRUNCATE);
#else
            strncpy(out->name, out->id, sizeof out->name - 1);
            out->name[sizeof out->name - 1] = '\0';
#endif
        }
        else
        {
#if defined(_MSC_VER)
            strncpy_s(out->name, sizeof out->name, "enemy", _TRUNCATE);
#else
            strncpy(out->name, "enemy", sizeof out->name - 1);
            out->name[sizeof out->name - 1] = '\0';
#endif
        }
    }
    json_find_int(buf, "\"group_min\"", &out->group_min);
    json_find_int(buf, "\"group_max\"", &out->group_max);
    if (out->group_max < out->group_min)
        out->group_max = out->group_min;
    json_find_int(buf, "\"patrol_radius\"", &out->patrol_radius);
    json_find_int(buf, "\"aggro_radius\"", &out->aggro_radius);
    json_find_float(buf, "\"speed\"", &out->speed);
    json_find_int(buf, "\"pop_target\"", &out->pop_target);
    json_find_int(buf, "\"xp_reward\"", &out->xp_reward);
    json_find_float(buf, "\"loot_chance\"", &out->loot_chance);
    json_find_int(buf, "\"base_level_offset\"", &out->base_level_offset);
    json_find_int(buf, "\"tier_id\"", &out->tier_id);
    json_find_int(buf, "\"archetype_id\"", &out->archetype_id);
    char idlep[128] = "", runp[128] = "", deathp[128] = "";
    extern int g_enemy_loader_skip_textures; /* use file-scope flag */
    if (json_find_string(buf, "\"idle_sheet\"", idlep, sizeof idlep))
    {
        if (!g_enemy_loader_skip_textures)
            load_sheet(idlep, &out->idle_tex, out->idle_frames, &out->idle_count);
    }
    if (json_find_string(buf, "\"run_sheet\"", runp, sizeof runp))
    {
        if (!g_enemy_loader_skip_textures)
            load_sheet(runp, &out->run_tex, out->run_frames, &out->run_count);
    }
    if (json_find_string(buf, "\"death_sheet\"", deathp, sizeof deathp))
    {
        if (!g_enemy_loader_skip_textures)
            load_sheet(deathp, &out->death_tex, out->death_frames, &out->death_count);
    }
    free(buf);
    return 1;
}

#ifdef _WIN32
#include <windows.h>
/**
 * @brief Loads enemy type definitions from JSON files in a directory (Windows implementation).
 *
 * Scans a directory for JSON files and loads enemy type definitions from each one.
 * Uses Windows API functions to enumerate files in the specified directory.
 * Automatically derives enemy IDs from filenames if not specified in JSON.
 *
 * @param dir_path Path to the directory containing JSON enemy definition files
 * @param types Array to store loaded enemy type definitions
 * @param max_types Maximum number of enemy types that can be stored in the array
 * @param out_count Pointer to store the number of enemy types successfully loaded
 * @return 1 if at least one enemy type loaded successfully, 0 on failure
 *
 * @note Only processes files with .json extension
 * @note Skips subdirectories and hidden files
 * @note Derives enemy ID from filename (without .json extension) if missing in JSON
 * @note Uses Windows FindFirstFile/FindNextFile API for directory enumeration
 */
int rogue_enemy_types_load_directory_json(const char* dir_path, RogueEnemyTypeDef types[],
                                          int max_types, int* out_count)
{
    if (!dir_path || !types || max_types <= 0)
    {
        if (out_count)
            *out_count = 0;
        return 0;
    }
    char pattern[512];
    snprintf(pattern, sizeof pattern, "%s\\*.json", dir_path);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        if (out_count)
            *out_count = 0;
        return 0;
    }
    int count = 0;
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (count >= max_types)
                break;
            char full[512];
            snprintf(full, sizeof full, "%s\\%s", dir_path, fd.cFileName);
            if (load_enemy_json_file(full, &types[count]))
            { /* derive id from filename if missing */
                if (types[count].id[0] == '\0')
                {
                    char* dot = strrchr(fd.cFileName, '.');
                    size_t base_len = dot ? (size_t) (dot - fd.cFileName) : strlen(fd.cFileName);
                    if (base_len >= sizeof(types[count].id))
                        base_len = sizeof(types[count].id) - 1;
                    memcpy(types[count].id, fd.cFileName, base_len);
                    types[count].id[base_len] = '\0';
                }
                count++;
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (out_count)
        *out_count = count;
    return count > 0;
}
#else
#include <dirent.h>
/**
 * @brief Loads enemy type definitions from JSON files in a directory (Unix implementation).
 *
 * Scans a directory for JSON files and loads enemy type definitions from each one.
 * Uses POSIX dirent API functions to enumerate files in the specified directory.
 * Automatically derives enemy IDs from filenames if not specified in JSON.
 *
 * @param dir_path Path to the directory containing JSON enemy definition files
 * @param types Array to store loaded enemy type definitions
 * @param max_types Maximum number of enemy types that can be stored in the array
 * @param out_count Pointer to store the number of enemy types successfully loaded
 * @return 1 if at least one enemy type loaded successfully, 0 on failure
 *
 * @note Only processes files with .json extension
 * @note Skips hidden files (starting with .)
 * @note Derives enemy ID from filename (without .json extension) if missing in JSON
 * @note Uses POSIX opendir/readdir/closedir API for directory enumeration
 */
int rogue_enemy_types_load_directory_json(const char* dir_path, RogueEnemyTypeDef types[],
                                          int max_types, int* out_count)
{
    if (!dir_path || !types || max_types <= 0)
    {
        if (out_count)
            *out_count = 0;
        return 0;
    }
    DIR* d = opendir(dir_path);
    if (!d)
    {
        if (out_count)
            *out_count = 0;
        return 0;
    }
    struct dirent* e;
    int count = 0;
    while ((e = readdir(d)))
    {
        if (e->d_name[0] == '.')
            continue;
        size_t n = strlen(e->d_name);
        if (n > 5 && strcmp(e->d_name + n - 5, ".json") == 0)
        {
            if (count >= max_types)
                break;
            char full[512];
            snprintf(full, sizeof full, "%s/%s", dir_path, e->d_name);
            if (load_enemy_json_file(full, &types[count]))
            {
                if (types[count].id[0] == '\0')
                {
                    size_t base_len = n - 5;
                    if (base_len >= sizeof(types[count].id))
                        base_len = sizeof(types[count].id) - 1;
                    memcpy(types[count].id, e->d_name, base_len);
                    types[count].id[base_len] = '\0';
                }
                count++;
            }
        }
    }
    closedir(d);
    if (out_count)
        *out_count = count;
    return count > 0;
}
#endif

/**
 * @brief Loads a sprite sheet and extracts individual animation frames.
 *
 * Loads a texture from file and automatically divides it into animation frames.
 * Assumes the sprite sheet is a horizontal strip of square frames.
 * Supports fallback path resolution for different directory structures.
 *
 * @param path Path to the sprite sheet image file
 * @param tex Pointer to RogueTexture structure to load the image into
 * @param frames Array to store individual frame definitions
 * @param out_count Pointer to store the number of frames extracted
 * @return 1 if sprite sheet loaded and frames extracted successfully, 0 on failure
 *
 * @note Assumes square frames (height = frame width)
 * @note Limits maximum frames to 8 to prevent excessive memory usage
 * @note Automatically tries "../" prefix if initial load fails
 * @note Frames are stored as sub-rectangles of the main texture
 */
static int load_sheet(const char* path, RogueTexture* tex, RogueSprite frames[], int* out_count)
{
    if (!rogue_texture_load(tex, path))
    {
        /* Attempt implicit ../ fallback if not already containing ../ and initial load failed */
        if (strncmp(path, "../", 3) != 0)
        {
            char alt[256];
            snprintf(alt, sizeof alt, "../%s", path);
            if (!rogue_texture_load(tex, alt))
                return 0; /* still fail */
        }
        else
            return 0;
    }
    int frame_size = tex->h; /* assume square */
    int count = tex->w / frame_size;
    if (count > 8)
        count = 8;
    if (count < 1)
        count = 1;
    for (int i = 0; i < count; i++)
    {
        frames[i].tex = tex;
        frames[i].sx = i * frame_size;
        frames[i].sy = 0;
        frames[i].sw = frame_size;
        frames[i].sh = tex->h;
    }
    *out_count = count;
    return 1;
}

/**
 * @brief Loads enemy type definitions from a configuration file.
 *
 * Parses a CSV-formatted configuration file containing enemy definitions.
 * Each line defines an enemy type with stats, behavior parameters, and sprite sheet paths.
 * Supports fallback path resolution for different directory structures.
 *
 * @param path Path to the enemy configuration file
 * @param types Array to store loaded enemy type definitions
 * @param inout_type_count Pointer to maximum capacity (input) and loaded count (output)
 * @return 1 if at least one enemy type loaded successfully, 0 on failure
 *
 * @note Expected CSV format:
 * ENEMY,name,group_min,group_max,patrol_radius,aggro_radius,speed,pop_target,xp_reward,loot_chance,idle.png,run.png,death.png
 * @note Lines starting with # are treated as comments
 * @note Automatically tries fallback paths (../, ../../, ../../../) if file not found
 * @note Validates and clamps loot_chance to [0,1] range
 * @note Sets sensible defaults for missing or invalid values
 */
int rogue_enemy_load_config(const char* path, RogueEnemyTypeDef types[], int* inout_type_count)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        /* Try fallback prefixes so running from build/ works (mirror player sheet behavior) */
        const char* prefixes[] = {"../", "../../", "../../../"};
        char attempt[512];
        for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]) && !f; i++)
        {
#if defined(_MSC_VER)
            _snprintf_s(attempt, sizeof attempt, _TRUNCATE, "%s%s", prefixes[i], path);
            fopen_s(&f, attempt, "rb");
#else
            snprintf(attempt, sizeof attempt, "%s%s", prefixes[i], path);
            f = fopen(attempt, "rb");
#endif
            if (f)
            {
                ROGUE_LOG_INFO("Opened enemy cfg via fallback path: %s", attempt);
                break;
            }
        }
    }
    if (!f)
    {
        ROGUE_LOG_WARN("enemy cfg open fail: %s", path);
        return 0;
    }
    char line[512];
    int loaded = 0;
    int cap = *inout_type_count;
    while (fgets(line, sizeof line, f))
    {
        char* p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;
        /* Format:
         * ENEMY,name,group_min,group_max,patrol_radius,aggro_radius,speed,pop_target,xp_reward,loot_chance,idle.png,run.png,death.png
         */
        if (strncmp(p, "ENEMY", 5) != 0)
            continue;
        p += 5;
        if (*p == ',')
            p++;
        char name[32];
        int gmin, gmax, pr, ar;
        float spd;
        int pop_target = 0, xp_reward = 1;
        float loot_chance = 0.0f;
        char idlep[128], runp[128], deathp[128];
#if defined(_MSC_VER)
        int n = sscanf_s(p, "%31[^,],%d,%d,%d,%d,%f,%d,%d,%f,%127[^,],%127[^,],%127[^\r\n]", name,
                         (unsigned) _countof(name), &gmin, &gmax, &pr, &ar, &spd, &pop_target,
                         &xp_reward, &loot_chance, idlep, (unsigned) _countof(idlep), runp,
                         (unsigned) _countof(runp), deathp, (unsigned) _countof(deathp));
#else
        int n = sscanf(p, "%31[^,],%d,%d,%d,%d,%f,%d,%d,%f,%127[^,],%127[^,],%127[^\r\n]", name,
                       &gmin, &gmax, &pr, &ar, &spd, &pop_target, &xp_reward, &loot_chance, idlep,
                       runp, deathp);
#endif
        if (n != 12)
        {
            ROGUE_LOG_WARN("enemy cfg parse fail: %s", line);
            continue;
        }
        if (loaded >= cap)
            break;
        RogueEnemyTypeDef* t = &types[loaded];
        memset(t, 0, sizeof *t);
#if defined(_MSC_VER)
        strncpy_s(t->name, sizeof t->name, name, _TRUNCATE);
#else
        strncpy(t->name, name, sizeof t->name - 1);
        t->name[sizeof t->name - 1] = '\0';
#endif
        t->group_min = gmin;
        t->group_max = gmax;
        t->patrol_radius = pr;
        t->aggro_radius = ar;
        t->speed = spd;
        t->weight = (pop_target > 0 ? pop_target : 1);
        t->pop_target = (pop_target > 0 ? pop_target : gmax * 4);
        t->xp_reward = (xp_reward > 0 ? xp_reward : 1);
        t->loot_chance = (loot_chance < 0 ? 0 : (loot_chance > 1 ? 1 : loot_chance));
        if (!load_sheet(idlep, &t->idle_tex, t->idle_frames, &t->idle_count))
            ROGUE_LOG_WARN("enemy idle sheet load fail: %s", idlep);
        if (!load_sheet(runp, &t->run_tex, t->run_frames, &t->run_count))
            ROGUE_LOG_WARN("enemy run sheet load fail: %s", runp);
        if (!load_sheet(deathp, &t->death_tex, t->death_frames, &t->death_count))
            ROGUE_LOG_WARN("enemy death sheet load fail: %s", deathp);
        loaded++;
    }
    fclose(f);
    if (loaded > 0)
    {
        ROGUE_LOG_INFO("Loaded %d enemy type(s)", loaded);
    }
    *inout_type_count = loaded;
    return loaded > 0;
}

/* Expose a simple setter to skip texture loads in headless/schema validation runs */
void rogue_enemy_loader_set_skip_textures(int skip)
{
    extern int g_enemy_loader_skip_textures;
    g_enemy_loader_skip_textures = (skip ? 1 : 0);
}
