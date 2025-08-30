/**
 * @file animation.c
 * @brief Animation system for loading and playing sprite animations, supporting
 *        Aseprite JSON metadata and fallback grid-based frame splitting.
 */

#include "animation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal JSON parsing for Aseprite frame list: we look for substrings
 * "frame":{..."x":N,"y":M,"w":W,"h":H} and "duration":D */

/**
 * @brief Finds the next occurrence of a key string in the given string.
 *
 * @param s The string to search in.
 * @param key The key string to find.
 * @return Pointer to the found key, or NULL if not found.
 */
static const char* find_next(const char* s, const char* key) { return strstr(s, key); }

/**
 * @brief Parses an integer value after a specified key in a JSON-like string.
 *
 * Looks for the key followed by optional whitespace, quotes, and colons,
 * then parses the following integer value.
 *
 * @param start The string to parse.
 * @param key The key to search for.
 * @param out Pointer to store the parsed integer value.
 * @return 1 if parsing was successful, 0 otherwise.
 */
static int parse_int_after(const char* start, const char* key, int* out)
{
    const char* p = strstr(start, key);
    if (!p)
        return 0;
    p += strlen(key);
    while (*p == ' ' || *p == '"' || *p == ':')
        p++;
    *out = atoi(p);
    return 1;
}

/**
 * @brief Loads an animation from PNG and optional JSON metadata files.
 *
 * Supports loading animations with Aseprite JSON frame data or falls back to
 * grid-based frame splitting if no JSON is provided. The animation can contain
 * up to 32 frames with individual durations.
 *
 * @param anim The animation structure to populate.
 * @param png_path Path to the PNG texture file.
 * @param json_path Path to the JSON metadata file (can be NULL for grid fallback).
 * @param fw Frame width for grid fallback (ignored if JSON is provided).
 * @param fh Frame height for grid fallback (ignored if JSON is provided).
 * @return true if the animation was loaded successfully, false otherwise.
 */
bool rogue_animation_load(RogueAnimation* anim, const char* png_path, const char* json_path, int fw,
                          int fh)
{
    memset(anim, 0, sizeof *anim);
    if (!rogue_texture_load(&anim->texture, png_path))
        return false;
    if (json_path)
    {
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, json_path, "rb");
#else
        f = fopen(json_path, "rb");
#endif
        if (f)
        {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            char* buf = (char*) malloc(sz + 1);
            if (!buf)
            {
                fclose(f);
                return false;
            }
            fread(buf, 1, sz, f);
            buf[sz] = '\0';
            fclose(f);
            const char* cursor = buf;
            int frame_index = 0;
            while (frame_index < 32)
            {
                const char* fr = find_next(cursor, "{\"frame\"");
                if (!fr)
                    break;
                int x, y, w, h, dur;
                if (!parse_int_after(fr, "\"x\"", &x))
                    break;
                parse_int_after(fr, "\"y\"", &y);
                parse_int_after(fr, "\"w\"", &w);
                parse_int_after(fr, "\"h\"", &h);
                const char* durk = find_next(fr, "\"duration\"");
                dur = 100;
                if (durk)
                    parse_int_after(durk, "\"duration\"", &dur);
                anim->frames[frame_index].sprite.tex = &anim->texture;
                anim->frames[frame_index].sprite.sx = x;
                anim->frames[frame_index].sprite.sy = y;
                anim->frames[frame_index].sprite.sw = w;
                anim->frames[frame_index].sprite.sh = h;
                anim->frames[frame_index].duration_ms = dur;
                anim->total_duration_ms += dur;
                frame_index++;
                cursor = fr + 10; /* advance */
            }
            anim->frame_count = frame_index;
            free(buf);
        }
    }
    if (anim->frame_count == 0)
    {
        /* fallback grid split */
        int cols = anim->texture.w / fw;
        int rows = anim->texture.h / fh;
        int idx = 0;
        for (int r = 0; r < rows && idx < 32; r++)
            for (int c = 0; c < cols && idx < 32; c++)
            {
                anim->frames[idx].sprite.tex = &anim->texture;
                anim->frames[idx].sprite.sx = c * fw;
                anim->frames[idx].sprite.sy = r * fh;
                anim->frames[idx].sprite.sw = fw;
                anim->frames[idx].sprite.sh = fh;
                anim->frames[idx].duration_ms = 100;
                anim->total_duration_ms += 100;
                idx++;
            }
        anim->frame_count = idx;
    }
    return anim->frame_count > 0;
}

/**
 * @brief Unloads an animation and frees associated resources.
 *
 * Destroys the texture and clears the animation structure.
 *
 * @param anim The animation to unload.
 */
void rogue_animation_unload(RogueAnimation* anim)
{
    rogue_texture_destroy(&anim->texture);
    memset(anim, 0, sizeof *anim);
}

/**
 * @brief Samples the animation to get the current frame based on elapsed time.
 *
 * Calculates which frame should be displayed at the given elapsed time by
 * accumulating frame durations. The animation loops continuously.
 *
 * @param anim The animation to sample.
 * @param elapsed_ms The elapsed time in milliseconds since animation start.
 * @return Pointer to the current animation frame, or NULL if animation has no frames.
 */
const RogueAnimFrame* rogue_animation_sample(const RogueAnimation* anim, int elapsed_ms)
{
    if (!anim->frame_count)
        return NULL;
    if (anim->total_duration_ms <= 0)
        return &anim->frames[0];
    int t = elapsed_ms % anim->total_duration_ms;
    int acc = 0;
    for (int i = 0; i < anim->frame_count; i++)
    {
        acc += anim->frames[i].duration_ms;
        if (t < acc)
            return &anim->frames[i];
    }
    return &anim->frames[anim->frame_count - 1];
}
