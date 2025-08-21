#include "game/weapon_pose.h"
#include "core/app_state.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#define ROGUE_WEAPON_POSE_MAX 32
#define FRAME_COUNT 8

typedef struct WeaponPoseSet
{
    int weapon_id;
    int loaded;
    RogueWeaponPoseFrame frames[FRAME_COUNT];
#ifdef ROGUE_HAVE_SDL
    SDL_Texture* textures[FRAME_COUNT];
    int tw[FRAME_COUNT];
    int th[FRAME_COUNT];
#endif
} WeaponPoseSet;

static WeaponPoseSet g_pose_sets[ROGUE_WEAPON_POSE_MAX];
static int g_pose_count = 0;

/* Directional pose set: 3 directions (down=0, up=1, side=2) each 8 frames */
typedef struct WeaponPoseDirSet
{
    int weapon_id;
    int loaded_dir[3]; /* 0/1 per direction JSON */
    RogueWeaponPoseFrame frames[3][FRAME_COUNT];
#ifdef ROGUE_HAVE_SDL
    SDL_Texture* texture_single;
    int tex_w, tex_h; /* shared weapon image (frame 0 texture) */
#endif
} WeaponPoseDirSet;

static WeaponPoseDirSet g_dir_sets[ROGUE_WEAPON_POSE_MAX];
static int g_dir_count = 0;

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

static int parse_float(const char* s, float* out)
{
    char* e = NULL;
    float v = (float) strtod(s, &e);
    if (e == s)
        return 0;
    *out = v;
    return 1;
}

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

static int load_json_pose(int weapon_id, WeaponPoseSet* set)
{
    char path[256];
    snprintf(path, sizeof path, "../assets/weapons/weapon_%d_pose.json", weapon_id);
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        ROGUE_LOG_DEBUG("weapon_pose_json_open_fail: %s", path);
        default_frames(set);
        return 0;
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

/* Parse a directional JSON (expects already opened file path string) */
static int load_json_pose_dir(int weapon_id, int dir_group, RogueWeaponPoseFrame* out_frames)
{
    /* dir suffix mapping */
    const char* suffix = (dir_group == 0) ? "down" : (dir_group == 1 ? "up" : "side");
    char path[256];
    snprintf(path, sizeof path, "../assets/weapons/weapon_%d_%s_pose.json", weapon_id, suffix);
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        ROGUE_LOG_DEBUG("weapon_pose_dir_json_open_fail: %s", path);
        default_frames_dir(out_frames);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 32768)
    {
        fclose(f);
        default_frames_dir(out_frames);
        return 0;
    }
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        default_frames_dir(out_frames);
        return 0;
    }
    if (fread(buf, 1, (size_t) sz, f) != (size_t) sz)
    {
        free(buf);
        fclose(f);
        default_frames_dir(out_frames);
        return 0;
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
    return frame_idx > 0;
}

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

const RogueWeaponPoseFrame* rogue_weapon_pose_get_dir(int weapon_id, int dir_group, int frame_index)
{
    WeaponPoseDirSet* ds = get_dir_set(weapon_id);
    if (!ds || dir_group < 0 || dir_group > 2 || !ds->loaded_dir[dir_group] || frame_index < 0 ||
        frame_index >= FRAME_COUNT)
        return NULL;
    return &ds->frames[dir_group][frame_index];
}

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

const RogueWeaponPoseFrame* rogue_weapon_pose_get(int weapon_id, int frame_index)
{
    WeaponPoseSet* set = get_set(weapon_id);
    if (!set || !set->loaded || frame_index < 0 || frame_index >= FRAME_COUNT)
        return NULL;
    return &set->frames[frame_index];
}

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
