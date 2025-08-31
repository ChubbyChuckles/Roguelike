/* dialogue_avatars.c - Avatar registry for dialogue speakers */
#include "../graphics/sprite.h"
#include "dialogue.h"
#include <stdio.h>
#include <string.h>

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* Avatar registry (speaker -> texture) */
#define ROGUE_DIALOGUE_MAX_AVATARS 32
typedef struct RogueDialogueAvatar
{
    char speaker[64];
    RogueTexture tex;
    int loaded;
} RogueDialogueAvatar;
static RogueDialogueAvatar g_avatars[ROGUE_DIALOGUE_MAX_AVATARS];
static int g_avatar_count = 0;

int rogue_dialogue_avatar_register(const char* speaker_id, const char* image_path)
{
#if defined(ROGUE_HAVE_SDL)
    if (!speaker_id || !*speaker_id || !image_path || !*image_path)
        return -1;
    for (int i = 0; i < g_avatar_count; i++)
        if (strcmp(g_avatars[i].speaker, speaker_id) == 0)
        {
            if (g_avatars[i].loaded)
            {
                rogue_texture_destroy(&g_avatars[i].tex);
                g_avatars[i].loaded = 0;
            }
            if (rogue_texture_load(&g_avatars[i].tex, image_path))
                g_avatars[i].loaded = 1;
            return g_avatars[i].loaded ? 0 : -2;
        }
    if (g_avatar_count >= ROGUE_DIALOGUE_MAX_AVATARS)
        return -3;
    RogueDialogueAvatar* av = &g_avatars[g_avatar_count++];
    memset(av, 0, sizeof *av);
    {
        size_t sl = strlen(speaker_id);
        if (sl > sizeof(av->speaker) - 1)
            sl = sizeof(av->speaker) - 1;
        memcpy(av->speaker, speaker_id, sl);
        av->speaker[sl] = '\0';
    }
    if (rogue_texture_load(&av->tex, image_path))
    {
        av->loaded = 1;
        return 0;
    }
    else
    {
        g_avatar_count--;
        return -4;
    }
#else
    (void) speaker_id;
    (void) image_path;
    return -10;
#endif
}

void rogue_dialogue_avatar_reset(void)
{
#if defined(ROGUE_HAVE_SDL)
    for (int i = 0; i < g_avatar_count; i++)
    {
        if (g_avatars[i].loaded)
        {
            rogue_texture_destroy(&g_avatars[i].tex);
            g_avatars[i].loaded = 0;
        }
    }
#endif
    g_avatar_count = 0;
}

RogueTexture* rogue_dialogue_avatar_get(const char* speaker_id)
{
#if defined(ROGUE_HAVE_SDL)
    if (!speaker_id)
        return NULL;
    for (int i = 0; i < g_avatar_count; i++)
        if (strcmp(g_avatars[i].speaker, speaker_id) == 0 && g_avatars[i].loaded)
            return &g_avatars[i].tex;
    return NULL;
#else
    (void) speaker_id;
    return NULL;
#endif
}

int rogue_dialogue_load_avatars_from_file(const char* path)
{
    if (!path)
        return -1;
    FILE* f = NULL;
    int loaded = 0;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0 || !f)
        return -2;
#else
    f = fopen(path, "rb");
    if (!f)
        return -2;
#endif
    char line[512];
    while (1)
    {
        char* got = fgets(line, sizeof line, f);
        if (!got)
            break;
        char* p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || !*p)
            continue;
        char* eq = strchr(p, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char* speaker = p;
        char* img = eq + 1; /* trim trail */
        char* nl = strchr(img, '\n');
        if (nl)
            *nl = '\0';
        char* cr = strchr(img, '\r');
        if (cr)
            *cr = '\0';
        /* trim spaces end */ int sl = (int) strlen(speaker);
        while (sl > 0 && (speaker[sl - 1] == ' ' || speaker[sl - 1] == '\t'))
            speaker[--sl] = '\0';
        while (*img == ' ' || *img == '\t')
            img++;
        int il = (int) strlen(img);
        while (il > 0 && (img[il - 1] == ' ' || img[il - 1] == '\t'))
            img[--il] = '\0';
        if (*speaker && *img)
        {
            if (rogue_dialogue_avatar_register(speaker, img) == 0)
                loaded++;
        }
    }
    fclose(f);
    return loaded; /* number of avatars loaded */
}
