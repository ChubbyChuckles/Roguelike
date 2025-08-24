#include "minimap.h"
#include "../util/log.h"
#include "minimap_loot_pings.h" /* 12.4 loot pings overlay */
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* Internal helpers (mirrors old static functions from app.c) */
static void ensure_minimap_target(int w, int h)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    if (w <= 0 || h <= 0)
        return;
    if (g_app.minimap_tex && (w != g_app.minimap_w || h != g_app.minimap_h))
    {
        SDL_DestroyTexture(g_app.minimap_tex);
        g_app.minimap_tex = NULL;
    }
    if (!g_app.minimap_tex)
    {
        g_app.minimap_tex = SDL_CreateTexture(g_app.renderer, SDL_PIXELFORMAT_RGBA8888,
                                              SDL_TEXTUREACCESS_TARGET, w, h);
        if (!g_app.minimap_tex)
        {
            ROGUE_LOG_WARN("minimap texture create %dx%d failed: %s", w, h, SDL_GetError());
            return;
        }
        SDL_SetTextureBlendMode(g_app.minimap_tex, SDL_BLENDMODE_BLEND);
        g_app.minimap_w = w;
        g_app.minimap_h = h;
        g_app.minimap_dirty = 1;
    }
#else
    (void) w;
    (void) h;
#endif
}

static void redraw_minimap_if_needed(int mm_w, int mm_h, int step)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.minimap_tex)
        return;
    static int frame_counter = 0;
    frame_counter++;
    int periodic = (frame_counter % 180) == 0; /* safety refresh every ~3s */
    if (!g_app.minimap_dirty && !periodic)
        return;
    SDL_Texture* prev = SDL_GetRenderTarget(g_app.renderer);
    SDL_SetRenderTarget(g_app.renderer, g_app.minimap_tex);
    SDL_SetRenderDrawColor(g_app.renderer, 0, 0, 0, 0);
    SDL_RenderClear(g_app.renderer);
    for (int y = 0; y < g_app.world_map.height; y += step)
    {
        for (int x = 0; x < g_app.world_map.width; x += step)
        {
            unsigned char t = g_app.world_map.tiles[y * g_app.world_map.width + x];
            RogueColor c = {0, 0, 0, 180};
            switch (t)
            {
            case ROGUE_TILE_WATER:
                c = (RogueColor){30, 90, 200, 220};
                break;
            case ROGUE_TILE_RIVER:
                c = (RogueColor){50, 140, 230, 220};
                break;
            case ROGUE_TILE_RIVER_WIDE:
                c = (RogueColor){70, 170, 250, 230};
                break;
            case ROGUE_TILE_RIVER_DELTA:
                c = (RogueColor){90, 190, 250, 230};
                break;
            case ROGUE_TILE_GRASS:
                c = (RogueColor){40, 160, 60, 220};
                break;
            case ROGUE_TILE_FOREST:
                c = (RogueColor){10, 90, 20, 220};
                break;
            case ROGUE_TILE_SWAMP:
                c = (RogueColor){50, 120, 50, 220};
                break;
            case ROGUE_TILE_MOUNTAIN:
                c = (RogueColor){120, 120, 120, 220};
                break;
            case ROGUE_TILE_SNOW:
                c = (RogueColor){230, 230, 240, 220};
                break;
            case ROGUE_TILE_CAVE_WALL:
                c = (RogueColor){60, 60, 60, 220};
                break;
            case ROGUE_TILE_CAVE_FLOOR:
                c = (RogueColor){110, 80, 60, 220};
                break;
            default:
                break;
            }
            SDL_SetRenderDrawColor(g_app.renderer, c.r, c.g, c.b, c.a);
            int mx = (int) ((float) x / (float) g_app.world_map.width * mm_w);
            int my = (int) ((float) y / (float) g_app.world_map.height * mm_h);
            SDL_Rect r = {mx, my, 1, 1};
            SDL_RenderFillRect(g_app.renderer, &r);
        }
    }
    g_app.minimap_dirty = 0;
    SDL_SetRenderTarget(g_app.renderer, prev);
#else
    (void) mm_w;
    (void) mm_h;
    (void) step;
#endif
}

void rogue_minimap_update_and_render(int mm_max_size)
{
    int mm_max = mm_max_size > 0 ? mm_max_size : 240;
    float scale_w = (float) mm_max / (float) g_app.world_map.width;
    float scale_h = (float) mm_max / (float) g_app.world_map.height;
    float mm_scale_f = (scale_w < scale_h) ? scale_w : scale_h;
    if (mm_scale_f < 0.5f)
        mm_scale_f = 0.5f;
    int mm_scale = (int) mm_scale_f;
    if (mm_scale < 1)
        mm_scale = 1;
    int mm_w = g_app.world_map.width * mm_scale;
    int mm_h = g_app.world_map.height * mm_scale;
    if (mm_w > mm_max)
        mm_w = mm_max;
    if (mm_h > mm_max)
        mm_h = mm_max;
    int mm_step = (g_app.world_map.width > 500 || g_app.world_map.height > 500) ? 2 : 1;
    ensure_minimap_target(mm_w, mm_h);
    redraw_minimap_if_needed(mm_w, mm_h, mm_step);
#ifdef ROGUE_HAVE_SDL
    if (g_app.minimap_tex && g_app.renderer)
    {
        int mm_x_off = g_app.viewport_w - mm_w - 8;
        int mm_y_off = 8;
        SDL_Rect dst = {mm_x_off, mm_y_off, mm_w, mm_h};
        SDL_RenderCopy(g_app.renderer, g_app.minimap_tex, NULL, &dst);
        g_app.frame_draw_calls++;
        SDL_SetRenderDrawColor(g_app.renderer, 255, 255, 255, 255);
        int pmx =
            mm_x_off + (int) ((g_app.player.base.pos.x / (float) g_app.world_map.width) * mm_w);
        int pmy =
            mm_y_off + (int) ((g_app.player.base.pos.y / (float) g_app.world_map.height) * mm_h);
        SDL_Rect mmpr = {pmx, pmy, 2, 2};
        SDL_RenderFillRect(g_app.renderer, &mmpr);
        g_app.frame_draw_calls++;
        /* 12.4 render loot pings after player marker */
        rogue_minimap_render_loot_pings(mm_x_off, mm_y_off, mm_w, mm_h);
    }
#endif
}
