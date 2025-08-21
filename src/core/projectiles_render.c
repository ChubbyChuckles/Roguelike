#include "core/app_state.h"
#include "core/projectiles_internal.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <math.h>

void rogue_projectiles_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    const int tsz = g_app.tile_size;
    float dt_ms = (float) g_app.dt * 1000.0f;
    rogue__update_impacts(dt_ms);
    rogue__update_shards(dt_ms);
    for (int i = 0; i < ROGUE_MAX_PROJECTILES; i++)
        if (g_projectiles[i].active)
        {
            RogueProjectile* p = &g_projectiles[i];
            float life_ratio = p->life_ms / p->max_life_ms;
            if (life_ratio < 0)
                life_ratio = 0;
            if (life_ratio > 1)
                life_ratio = 1;
            float pulse = 0.5f + 0.5f * sinf((p->anim_t) * 0.02f * 6.283185f);
            float fade = 1.0f - life_ratio;
            float size = 8.0f + 4.0f * pulse;
            Uint8 r = (Uint8) (200 + 55 * pulse);
            Uint8 g = (Uint8) (80 + 60 * (1.0f - pulse));
            Uint8 b = 40;
            Uint8 a = (Uint8) (180 + 75 * pulse);
            a = (Uint8) (a * fade);
            int px = (int) (p->x * tsz - g_app.cam_x);
            int py = (int) (p->y * tsz - g_app.cam_y);
            SDL_SetRenderDrawColor(g_app.renderer, r, g, b, a);
            SDL_Rect core = {(int) (px - size * 0.5f), (int) (py - size * 0.5f), (int) size,
                             (int) size};
            SDL_RenderFillRect(g_app.renderer, &core);
            SDL_SetRenderDrawColor(g_app.renderer, 255, 200, 120, (Uint8) (220 * fade));
            SDL_Rect inner = {core.x + core.w / 4, core.y + core.h / 4, core.w / 2, core.h / 2};
            SDL_RenderFillRect(g_app.renderer, &inner);
            for (int h = 0; h < p->hcount; ++h)
            {
                float t = (float) (h + 1) / (float) (ROGUE_PROJECTILE_HISTORY + 1);
                int hx = (int) (p->hx[h] * tsz - g_app.cam_x);
                int hy = (int) (p->hy[h] * tsz - g_app.cam_y);
                float hs = size * (0.6f - 0.05f * h);
                if (hs < 2)
                    hs = 2;
                Uint8 ha = (Uint8) (a * (0.4f * (1.0f - t)));
                SDL_SetRenderDrawColor(g_app.renderer, (Uint8) (r * 0.8f), (Uint8) (g * 0.6f), b,
                                       ha);
                SDL_Rect tr = {(int) (hx - hs * 0.5f), (int) (hy - hs * 0.5f), (int) hs, (int) hs};
                SDL_RenderFillRect(g_app.renderer, &tr);
            }
        }
    for (int i = 0; i < ROGUE_MAX_IMPACT_BURSTS; i++)
        if (g_impacts[i].active)
        {
            float norm = 1.0f - (g_impacts[i].life_ms / g_impacts[i].total_ms);
            if (norm < 0)
                norm = 0;
            if (norm > 1)
                norm = 1;
            float base_size = 10.0f;
            float radius = base_size + norm * 28.0f;
            int px = (int) (g_impacts[i].x * tsz - g_app.cam_x);
            int py = (int) (g_impacts[i].y * tsz - g_app.cam_y);
            Uint8 alpha_outer = (Uint8) (180 * (1.0f - norm));
            Uint8 alpha_inner = (Uint8) (255 * (1.0f - norm * norm));
            SDL_SetRenderDrawColor(g_app.renderer, 255, 160, 80, alpha_outer);
            SDL_Rect outer = {(int) (px - radius), (int) (py - radius), (int) (radius * 2),
                              (int) (radius * 2)};
            SDL_RenderFillRect(g_app.renderer, &outer);
            SDL_SetRenderDrawColor(g_app.renderer, 255, 220, 120, alpha_inner);
            float inner_r = radius * 0.5f;
            SDL_Rect inner = {(int) (px - inner_r), (int) (py - inner_r), (int) (inner_r * 2),
                              (int) (inner_r * 2)};
            SDL_RenderFillRect(g_app.renderer, &inner);
        }
    for (int i = 0; i < ROGUE_MAX_SHARDS; i++)
        if (g_shards[i].active)
        {
            float norm =
                g_shards[i].life_ms / (g_shards[i].total_ms > 0 ? g_shards[i].total_ms : 1.0f);
            if (norm < 0)
                norm = 0;
            if (norm > 1)
                norm = 1;
            float fade = norm;
            int px = (int) (g_shards[i].x * tsz - g_app.cam_x);
            int py = (int) (g_shards[i].y * tsz - g_app.cam_y);
            float s = g_shards[i].size * (0.3f + 0.7f * norm);
            Uint8 a = (Uint8) (200 * fade);
            Uint8 r = (Uint8) (255);
            Uint8 gcol = (Uint8) (120 + 80 * (1.0f - norm));
            Uint8 b = (Uint8) (50);
            SDL_SetRenderDrawColor(g_app.renderer, r, gcol, b, a);
            SDL_Rect shard = {(int) (px - s * 0.5f), (int) (py - s * 0.5f), (int) s, (int) s};
            SDL_RenderFillRect(g_app.renderer, &shard);
        }
#endif
}
