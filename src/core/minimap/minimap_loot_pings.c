#include "minimap_loot_pings.h"
#include "../app/app_state.h"
#include "../loot/loot_rarity.h"
#include <string.h>

#ifndef ROGUE_MINIMAP_PING_CAP
#define ROGUE_MINIMAP_PING_CAP 128
#endif

typedef struct RogueMinimapPing
{
    float x, y;
    float life_ms;
    int active;
    int rarity;
} RogueMinimapPing;
static RogueMinimapPing g_pings[ROGUE_MINIMAP_PING_CAP];
static float g_ping_lifetime_ms = 5000.0f; /* 5s default */

void rogue_minimap_pings_reset(void) { memset(g_pings, 0, sizeof g_pings); }

int rogue_minimap_ping_loot(float x, float y, int rarity)
{
    if (rarity < 0)
        rarity = 0;
    if (rarity > 4)
        rarity = 4;
    for (int i = 0; i < ROGUE_MINIMAP_PING_CAP; i++)
        if (!g_pings[i].active)
        {
            g_pings[i].x = x;
            g_pings[i].y = y;
            g_pings[i].life_ms = 0;
            g_pings[i].active = 1;
            g_pings[i].rarity = rarity;
            return i;
        }
    return -1; /* full */
}

void rogue_minimap_pings_update(float dt_ms)
{
    for (int i = 0; i < ROGUE_MINIMAP_PING_CAP; i++)
        if (g_pings[i].active)
        {
            g_pings[i].life_ms += dt_ms;
            if (g_pings[i].life_ms >= g_ping_lifetime_ms)
                g_pings[i].active = 0;
        }
}

int rogue_minimap_pings_active_count(void)
{
    int c = 0;
    for (int i = 0; i < ROGUE_MINIMAP_PING_CAP; i++)
        if (g_pings[i].active)
            c++;
    return c;
}

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* Simple render hook used inside minimap after base map drawn. */
void rogue_minimap_render_loot_pings(int mm_x_off, int mm_y_off, int mm_w, int mm_h)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    if (!g_app.world_map.width || !g_app.world_map.height)
        return;
    for (int i = 0; i < ROGUE_MINIMAP_PING_CAP; i++)
        if (g_pings[i].active)
        {
            float nx = g_pings[i].x / (float) g_app.world_map.width;
            float ny = g_pings[i].y / (float) g_app.world_map.height;
            int px = mm_x_off + (int) (nx * mm_w);
            int py = mm_y_off + (int) (ny * mm_h);
            RogueRarityColor c = rogue_rarity_color((RogueItemRarity) g_pings[i].rarity);
            /* fade alpha near end of life */
            float t = g_pings[i].life_ms / g_ping_lifetime_ms;
            if (t > 1)
                t = 1;
            float fade = (t < 0.8f) ? 1.0f : (1.0f - (t - 0.8f) / 0.2f);
            if (fade < 0)
                fade = 0;
            SDL_SetRenderDrawColor(g_app.renderer, c.r, c.g, c.b, (Uint8) (200 * fade));
            SDL_Rect r = {px - 1, py - 1, 3, 3};
            SDL_RenderFillRect(g_app.renderer, &r);
            g_app.frame_draw_calls++;
        }
#else
    (void) mm_x_off;
    (void) mm_y_off;
    (void) mm_w;
    (void) mm_h;
#endif
}
