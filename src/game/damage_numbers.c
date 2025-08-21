#include "game/damage_numbers.h"
#include "core/app_state.h"
#include "graphics/font.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit)
{
    if (amount == 0)
        return;
    /* Simple spatial batching: if a recent number within small radius (<0.4 tiles) and same
     * ownership, accumulate. */
    for (int i = 0; i < g_app.dmg_number_count; i++)
    {
        float dx = g_app.dmg_numbers[i].x - x;
        float dy = g_app.dmg_numbers[i].y - y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 < 0.16f && g_app.dmg_numbers[i].from_player == from_player &&
            g_app.dmg_numbers[i].crit == crit)
        {
            g_app.dmg_numbers[i].amount += amount; /* refresh life slightly */
            float extend = 120.0f;
            g_app.dmg_numbers[i].life_ms += extend;
            if (g_app.dmg_numbers[i].life_ms > g_app.dmg_numbers[i].total_ms)
                g_app.dmg_numbers[i].life_ms = g_app.dmg_numbers[i].total_ms;
            return;
        }
    }
    if (g_app.dmg_number_count < (int) (sizeof(g_app.dmg_numbers) / sizeof(g_app.dmg_numbers[0])))
    {
        int i = g_app.dmg_number_count++;
        g_app.dmg_numbers[i].x = x;
        g_app.dmg_numbers[i].y = y;
        g_app.dmg_numbers[i].vx = 0.0f;
        g_app.dmg_numbers[i].vy = -0.38f;
        g_app.dmg_numbers[i].life_ms = 700.0f;
        g_app.dmg_numbers[i].total_ms = 700.0f;
        g_app.dmg_numbers[i].amount = amount;
        g_app.dmg_numbers[i].from_player = from_player;
        g_app.dmg_numbers[i].crit = crit ? 1 : 0;
        g_app.dmg_numbers[i].scale = crit ? 1.4f : 1.0f;
        g_app.dmg_numbers[i].spawn_ms = (float) g_app.game_time_ms;
        g_app.dmg_numbers[i].alpha = 1.0f;
    }
}
void rogue_add_damage_number(float x, float y, int amount, int from_player)
{
    rogue_add_damage_number_ex(x, y, amount, from_player, 0);
}
int rogue_app_damage_number_count(void) { return g_app.dmg_number_count; }
void rogue_app_test_decay_damage_numbers(float ms)
{
    for (int i = 0; i < g_app.dmg_number_count;)
    {
        g_app.dmg_numbers[i].life_ms -= ms;
        if (g_app.dmg_numbers[i].life_ms <= 0)
        {
            g_app.dmg_numbers[i] = g_app.dmg_numbers[g_app.dmg_number_count - 1];
            g_app.dmg_number_count--;
            continue;
        }
        ++i;
    }
}

void rogue_damage_numbers_update(float dt_seconds)
{
    if (g_app.dmg_number_count <= 0)
        return;
    float dt_ms = dt_seconds * 1000.0f;
    for (int i = 0; i < g_app.dmg_number_count;)
    {
        g_app.dmg_numbers[i].life_ms -= dt_ms;
        g_app.dmg_numbers[i].x += g_app.dmg_numbers[i].vx * dt_seconds;
        g_app.dmg_numbers[i].y += g_app.dmg_numbers[i].vy * dt_seconds;
        g_app.dmg_numbers[i].vy -= 0.15f * dt_seconds;
        /* Fade curve: early ease-in (first 80ms ramp alpha), late quadratic fade last 150ms */
        float age = g_app.dmg_numbers[i].total_ms - g_app.dmg_numbers[i].life_ms;
        float alpha = 1.0f;
        if (age < 80.0f)
            alpha = age / 80.0f;
        else
        {
            if (g_app.dmg_numbers[i].life_ms < 150.0f)
            {
                float t = g_app.dmg_numbers[i].life_ms / 150.0f;
                alpha = t * t;
            }
        }
        g_app.dmg_numbers[i].alpha = alpha;
        if (g_app.dmg_numbers[i].life_ms <= 0)
        {
            g_app.dmg_numbers[i] = g_app.dmg_numbers[g_app.dmg_number_count - 1];
            g_app.dmg_number_count--;
            continue;
        }
        ++i;
    }
}

void rogue_damage_numbers_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    for (int i = 0; i < g_app.dmg_number_count; i++)
    {
        int alpha =
            (int) (255 * (g_app.dmg_numbers[i].alpha < 0
                              ? 0
                              : (g_app.dmg_numbers[i].alpha > 1 ? 1 : g_app.dmg_numbers[i].alpha)));
        if (alpha < 0)
            alpha = 0;
        if (alpha > 255)
            alpha = 255;
        int screen_x = (int) (g_app.dmg_numbers[i].x * g_app.tile_size - g_app.cam_x);
        int screen_y = (int) (g_app.dmg_numbers[i].y * g_app.tile_size - g_app.cam_y);
        char buf[16];
        snprintf(buf, sizeof buf, "%d", g_app.dmg_numbers[i].amount);
        RogueColor col;
        if (g_app.dmg_numbers[i].crit)
        {
            col = (RogueColor){255, 255, 120, (unsigned char) alpha};
        }
        else if (g_app.dmg_numbers[i].from_player)
        {
            col = (RogueColor){255, 210, 40, (unsigned char) alpha};
        }
        else
        {
            col = (RogueColor){255, 60, 60, (unsigned char) alpha};
        }
        int txt_scale =
            g_app.dmg_numbers[i].scale > 0 ? (int) (g_app.dmg_numbers[i].scale * 1.0f) : 1;
        if (txt_scale < 1)
            txt_scale = 1;
        if (txt_scale > 4)
            txt_scale = 4;
        rogue_font_draw_text(screen_x, screen_y, buf, txt_scale, col);
    }
#endif
}
