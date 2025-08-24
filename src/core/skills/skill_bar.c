#include "core/skill_bar.h"
#include "core/app_state.h"
#include "core/skills.h"
#include "graphics/font.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <string.h>

void rogue_skill_bar_set_slot(int slot, int skill_id)
{
    if (slot >= 0 && slot < 10)
        g_app.skill_bar[slot] = skill_id;
}
int rogue_skill_bar_get_slot(int slot)
{
    if (slot >= 0 && slot < 10)
        return g_app.skill_bar[slot];
    return -1;
}

static float g_slot_flash_ms[10] = {0};
void rogue_skill_bar_flash(int slot)
{
    if (slot >= 0 && slot < 10)
        g_slot_flash_ms[slot] = 180.0f;
}

void rogue_skill_bar_update(float dt_ms)
{
    for (int i = 0; i < 10; i++)
    {
        if (g_slot_flash_ms[i] > 0)
        {
            g_slot_flash_ms[i] -= dt_ms;
            if (g_slot_flash_ms[i] < 0)
                g_slot_flash_ms[i] = 0;
        }
    }
}

void rogue_skill_bar_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    int bar_w = 10 * 34 + 8;
    int bar_h = 46;
    int bar_x = 4;
    int bar_y = g_app.viewport_h - bar_h - 4;
    SDL_SetRenderDrawColor(g_app.renderer, 20, 20, 32, 210);
    SDL_Rect bg = {bar_x, bar_y, bar_w, bar_h};
    SDL_RenderFillRect(g_app.renderer, &bg);
    SDL_SetRenderDrawColor(g_app.renderer, 80, 80, 120, 255);
    SDL_Rect top = {bar_x, bar_y, bar_w, 2};
    SDL_RenderFillRect(g_app.renderer, &top);
    for (int i = 0; i < 10; i++)
    {
        int slot_x = bar_x + 6 + i * 34;
        SDL_Rect cell = {slot_x, bar_y + 6, 32, 32};
        int flash = g_slot_flash_ms[i] > 0;
        int skill_id = g_app.skill_bar[i];
        const RogueSkillDef* def = rogue_skill_get_def(skill_id);
        SDL_SetRenderDrawColor(g_app.renderer, flash ? 200 : (def ? 60 : 30),
                               flash ? 180 : (def ? 60 : 30), flash ? 40 : (def ? 80 : 30), 255);
        SDL_RenderFillRect(g_app.renderer, &cell);
        if (def)
        {
            const RogueSkillState* st = rogue_skill_get_state(skill_id);
            int rank = st->rank;
            /* Draw icon if available */
#ifdef ROGUE_HAVE_SDL
            if (g_app.skill_icon_textures && skill_id >= 0 && skill_id < g_app.skill_count)
            {
                RogueTexture* tex = &g_app.skill_icon_textures[skill_id];
                if (tex->handle)
                {
                    SDL_Rect dst = {cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4};
                    SDL_RenderCopy(g_app.renderer, tex->handle, NULL, &dst);
                }
            }
#endif
            /* Fallback letter if no icon texture */
            if (!g_app.skill_icon_textures || skill_id < 0 || skill_id >= g_app.skill_count ||
                !g_app.skill_icon_textures[skill_id].handle)
            {
                char icon_letter[2] = {'?', 0};
                if (def->name && def->name[0])
                    icon_letter[0] = def->name[0];
                rogue_font_draw_text(cell.x + 10, cell.y + 4, icon_letter, 1,
                                     (RogueColor){220, 220, 255, 255});
            }
            char label[5];
            snprintf(label, sizeof label, "%d", rank);
            rogue_font_draw_text(cell.x + 18, cell.y + 20, label, 1,
                                 (RogueColor){255, 255, 200, 255});
            /* Cooldown overlay */
            double now = g_app.game_time_ms;
            if (st->cooldown_end_ms > now)
            {
                double remain = st->cooldown_end_ms - now;
                /* compute total cooldown (respect test override) */
                float cd_total;
#ifdef ROGUE_TEST_SHORT_COOLDOWNS
                cd_total = 1000.0f;
#else
                cd_total = def->base_cooldown_ms - (rank - 1) * def->cooldown_reduction_ms_per_rank;
                if (cd_total < 100)
                    cd_total = 100;
#endif
                float frac = (float) (remain / cd_total);
                if (frac < 0)
                    frac = 0;
                if (frac > 1)
                    frac = 1;
                int overlay_h = (int) (frac * (float) cell.h);
                SDL_SetRenderDrawColor(g_app.renderer, 0, 0, 0, 130);
                SDL_Rect ov = {cell.x, cell.y, cell.w, overlay_h};
                SDL_RenderFillRect(g_app.renderer, &ov);
                char rbuf[8];
                int ms_int = (int) (remain / 1000.0 + 0.999);
                snprintf(rbuf, sizeof rbuf, "%d", ms_int);
                rogue_font_draw_text(cell.x + 8, cell.y + 12, rbuf, 1,
                                     (RogueColor){255, 120, 120, 255});
            }
        }
        char idx[3];
        snprintf(idx, sizeof idx, "%d", (i + 1) % 10);
        rogue_font_draw_text(slot_x + 10, bar_y + 40, idx, 1, (RogueColor){200, 200, 255, 255});
    }
#endif
}
