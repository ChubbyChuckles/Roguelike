#include "core/item_sprite.h"
#include "util/log.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#ifdef ROGUE_HAVE_SDL_IMAGE
#include <SDL_image.h>
#endif
#endif

int rogue_item_sprite_load_atlas(const char* path)
{
    /* With per-item PNGs this becomes a no-op kept for compatibility. Optionally verify one sample
     * file exists. */
    (void) path;
#ifdef ROGUE_HAVE_SDL
    if (path)
    {
        SDL_RWops* rw = SDL_RWFromFile(path, "rb");
        if (rw)
        {
            SDL_RWclose(rw);
            ROGUE_LOG_DEBUG("item_sprite_atlas_probe_ok: %s", path);
        }
        else
        {
            ROGUE_LOG_WARN("item_sprite_atlas_probe_fail: %s", path);
        }
    }
#else
    ROGUE_LOG_DEBUG("item_sprite_atlas_noop_no_sdl: %s", path ? path : "<null>");
#endif
    return 0;
}
