#include "asset_config.h"
#include "../core/app/app_state.h"
#include "log.h"
#ifdef ROGUE_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif
#include <stdio.h>
#include <string.h>

void rogue_asset_load_sounds(void)
{
#ifdef ROGUE_HAVE_SDL_MIXER
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "assets/sounds.cfg", "rb");
#else
    f = fopen("assets/sounds.cfg", "rb");
#endif
    if (!f)
        return; /* optional */
    char line[512];
    while (fgets(line, sizeof line, f))
    {
        char* p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;
        if (strncmp(p, "LEVELUP", 7) == 0)
        {
            p += 7;
            if (*p == ',')
                p++;
            while (*p == ' ' || *p == '\t')
                p++;
            char path_tok[400];
            int i = 0;
            while (p[i] && p[i] != '\n' && i < 399)
            {
                path_tok[i] = p[i];
                i++;
            }
            path_tok[i] = '\0';
            if (g_app.sfx_levelup)
            {
                Mix_FreeChunk(g_app.sfx_levelup);
                g_app.sfx_levelup = NULL;
            }
            g_app.sfx_levelup = Mix_LoadWAV(path_tok);
            if (!g_app.sfx_levelup)
            {
                ROGUE_LOG_WARN("Failed to load levelup sound: %s", path_tok);
            }
            else
            {
                ROGUE_LOG_INFO("Loaded levelup sound: %s", path_tok);
            }
        }
    }
    fclose(f);
#endif
}
