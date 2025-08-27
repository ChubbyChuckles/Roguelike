#include "hud.h"
#include "../../graphics/font.h"
#include "../enemy/enemy_difficulty_scaling.h" /* ΔL severity classifier */
#include "hud_bars.h"                          /* Phase 6.2 layered bar smoothing */
#include "hud_buff_belt.h"                     /* Phase 6.3 buff belt */
#include "hud_layout.h"                        /* Phase 6.1 data-driven HUD layout */
#include "hud_overlays.h"                      /* Phase 6.6 alerts + 6.7 metrics */
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/** @brief Persistent smoothing state for HUD bars. */
static RogueHUDBarsState g_hud_bars_state; /* persistent smoothing */
/** @brief Buff overlay state for the HUD buff belt. */
static RogueHUDBuffBeltState g_hud_buff_belt; /* buff overlay */

/** @brief Main HUD rendering function.
 *
 * Renders the complete Heads-Up Display including health, mana, XP, and AP bars,
 * level text, buff belt, alerts, metrics overlay, and enemy difficulty indicator.
 * Uses data-driven layout and smoothing for bars. Only renders if SDL renderer is available.
 */
void rogue_hud_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;

    // Load layout on first use (lazy) - test harness can call loader explicitly.
    const RogueHUDLayout* lay = rogue_hud_layout();

    // Update smoothing state (assume ~16ms if unknown frame delta; using fixed 16 for simplicity
    // here)
    rogue_hud_bars_update(&g_hud_bars_state, g_app.player.health, g_app.player.max_health,
                          g_app.player.mana, g_app.player.max_mana, g_app.player.action_points,
                          g_app.player.max_action_points, 16);

    // Health bar
    int hp_w = lay->health.w, hp_h = lay->health.h, hp_x = lay->health.x, hp_y = lay->health.y;
    float hp_primary = rogue_hud_health_primary(&g_hud_bars_state);
    float hp_secondary = rogue_hud_health_secondary(&g_hud_bars_state);
    SDL_SetRenderDrawColor(g_app.renderer, 40, 12, 12, 255);
    SDL_Rect hbgb = {hp_x - 2, hp_y - 2, hp_w + 4, hp_h + 4};
    SDL_RenderFillRect(g_app.renderer, &hbgb);
    // Secondary (lag) bar in darker tone behind
    SDL_SetRenderDrawColor(g_app.renderer, 120, 25, 25, 255);
    SDL_Rect hblag = {hp_x, hp_y, (int) (hp_w * hp_secondary), hp_h};
    SDL_RenderFillRect(g_app.renderer, &hblag);
    SDL_SetRenderDrawColor(g_app.renderer, 200, 40, 40, 255);
    SDL_Rect hbpri = {hp_x, hp_y, (int) (hp_w * hp_primary), hp_h};
    SDL_RenderFillRect(g_app.renderer, &hbpri);

    // Mana bar
    int mp_w = lay->mana.w, mp_h = lay->mana.h, mp_x = lay->mana.x, mp_y = lay->mana.y;
    float mp_primary = rogue_hud_mana_primary(&g_hud_bars_state);
    float mp_secondary = rogue_hud_mana_secondary(&g_hud_bars_state);
    SDL_SetRenderDrawColor(g_app.renderer, 10, 18, 40, 255);
    SDL_Rect mpbgb = {mp_x - 2, mp_y - 2, mp_w + 4, mp_h + 4};
    SDL_RenderFillRect(g_app.renderer, &mpbgb);
    SDL_SetRenderDrawColor(g_app.renderer, 25, 55, 150, 255);
    SDL_Rect mplag = {mp_x, mp_y, (int) (mp_w * mp_secondary), mp_h};
    SDL_RenderFillRect(g_app.renderer, &mplag);
    SDL_SetRenderDrawColor(g_app.renderer, 60, 110, 230, 255);
    SDL_Rect mppri = {mp_x, mp_y, (int) (mp_w * mp_primary), mp_h};
    SDL_RenderFillRect(g_app.renderer, &mppri);

    // XP bar (unchanged styling for now)
    int xp_w = lay->xp.w, xp_h = lay->xp.h, xp_x = lay->xp.x, xp_y = lay->xp.y;
    float xp_ratio = (g_app.player.xp_to_next > 0)
                         ? (float) g_app.player.xp / (float) g_app.player.xp_to_next
                         : 0.0f;
    if (xp_ratio < 0)
        xp_ratio = 0;
    if (xp_ratio > 1)
        xp_ratio = 1;
    SDL_SetRenderDrawColor(g_app.renderer, 25, 25, 25, 255);
    SDL_Rect xpbgb = {xp_x - 2, xp_y - 2, xp_w + 4, xp_h + 4};
    SDL_RenderFillRect(g_app.renderer, &xpbgb);
    SDL_SetRenderDrawColor(g_app.renderer, 90, 60, 10, 255);
    SDL_Rect xpbf1 = {xp_x, xp_y, (int) (xp_w * xp_ratio), xp_h};
    SDL_RenderFillRect(g_app.renderer, &xpbf1);
    SDL_SetRenderDrawColor(g_app.renderer, 200, 140, 30, 255);
    SDL_Rect xpbf2 = {xp_x, xp_y, (int) (xp_w * xp_ratio * 0.55f), xp_h};
    SDL_RenderFillRect(g_app.renderer, &xpbf2);

    // AP bar (new) directly below XP bar with small gap
    int ap_gap = 4;
    int ap_h = 6;
    int ap_w = xp_w;
    int ap_x = xp_x;
    int ap_y = xp_y + xp_h + ap_gap;
    float ap_primary = rogue_hud_ap_primary(&g_hud_bars_state);
    float ap_secondary = rogue_hud_ap_secondary(&g_hud_bars_state);
    SDL_SetRenderDrawColor(g_app.renderer, 18, 18, 36, 255);
    SDL_Rect apgb = {ap_x - 2, ap_y - 2, ap_w + 4, ap_h + 4};
    SDL_RenderFillRect(g_app.renderer, &apgb);
    SDL_SetRenderDrawColor(g_app.renderer, 35, 95, 95, 255);
    SDL_Rect aplag = {ap_x, ap_y, (int) (ap_w * ap_secondary), ap_h};
    SDL_RenderFillRect(g_app.renderer, &aplag);
    SDL_SetRenderDrawColor(g_app.renderer, 60, 180, 180, 255);
    SDL_Rect appri = {ap_x, ap_y, (int) (ap_w * ap_primary), ap_h};
    SDL_RenderFillRect(g_app.renderer, &appri);

    // Level text
    char lvlbuf[32];
    snprintf(lvlbuf, sizeof lvlbuf, "Lv %d", g_app.player.level);
    rogue_font_draw_text(lay->level_text_x, lay->level_text_y, lvlbuf, 1,
                         (RogueColor){255, 255, 180, 255});

    // After bars & level text, render buff belt center-top
    rogue_hud_buff_belt_refresh(&g_hud_buff_belt, g_app.game_time_ms);
    rogue_hud_buff_belt_render(&g_hud_buff_belt, g_app.viewport_w);
    /* Alerts (Phase 6.6) rendered after primary HUD so they overlay center-top */
    rogue_alerts_update_and_render(16.0f);
    /* Metrics overlay (Phase 6.7) bottom-left */
    rogue_metrics_overlay_render();
    /* Enemy ΔL severity indicator (Enemy Difficulty Phase 1.6) */
    if (g_app.target_enemy_active)
    {
        int player_level = g_app.player.level;
        int enemy_level = g_app.target_enemy_level;
        RogueEnemyDeltaLSeverity sev =
            rogue_enemy_difficulty_classify_delta(player_level, enemy_level);
        int dl = player_level - enemy_level; /* positive => player higher */
        char buf[48];
        snprintf(buf, sizeof buf, "ΔL %d", dl);
        int x = lay->level_text_x + 60;
        int y = lay->level_text_y; /* offset near level */
        unsigned char r = 200, g = 200, b = 200;
        switch (sev)
        {
        case ROGUE_DLVL_EQUAL:
            r = 200;
            g = 200;
            b = 200;
            break;
        case ROGUE_DLVL_MINOR:
            r = 120;
            g = 220;
            b = 120;
            break;
        case ROGUE_DLVL_MODERATE:
            r = 255;
            g = 210;
            b = 80;
            break;
        case ROGUE_DLVL_MAJOR:
            r = 255;
            g = 120;
            b = 60;
            break;
        case ROGUE_DLVL_DOMINANCE:
            r = 80;
            g = 200;
            b = 80;
            break; /* strong advantage */
        case ROGUE_DLVL_TRIVIAL:
            r = 40;
            g = 140;
            b = 40;
            break; /* trivialized */
        }
        rogue_font_draw_text(x, y, buf, 1, (RogueColor){r, g, b, 255});
    }
#endif
}

/** @brief Renders the player statistics panel.
 *
 * Displays a panel showing player stats including STR, DEX, VIT, INT, CRIT%, and CRITDMG.
 * The panel includes bars for visual representation and highlights the currently selected stat.
 * Only renders if the stats panel is enabled and SDL renderer is available.
 */
void rogue_stats_panel_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.show_stats_panel)
        return;
    if (!g_app.renderer || g_app.headless)
        return;

    SDL_Rect panel = {160, 70, 200, 180};
    SDL_SetRenderDrawColor(g_app.renderer, 12, 12, 28, 235);
    SDL_RenderFillRect(g_app.renderer, &panel);

    // Border
    SDL_SetRenderDrawColor(g_app.renderer, 90, 90, 140, 255);
    SDL_Rect btop = {panel.x - 2, panel.y - 2, panel.w + 4, 2};
    SDL_RenderFillRect(g_app.renderer, &btop);
    SDL_Rect bbot = {panel.x - 2, panel.y + panel.h, panel.w + 4, 2};
    SDL_RenderFillRect(g_app.renderer, &bbot);
    SDL_Rect bl = {panel.x - 2, panel.y, 2, panel.h};
    SDL_RenderFillRect(g_app.renderer, &bl);
    SDL_Rect br = {panel.x + panel.w, panel.y, 2, panel.h};
    SDL_RenderFillRect(g_app.renderer, &br);

    // Header gradient
    SDL_SetRenderDrawColor(g_app.renderer, 130, 50, 170, 255);
    SDL_Rect hdr = {panel.x, panel.y, panel.w, 16};
    SDL_RenderFillRect(g_app.renderer, &hdr);
    SDL_SetRenderDrawColor(g_app.renderer, 180, 80, 220, 255);
    SDL_Rect hdr2 = {panel.x, panel.y, panel.w / 2, 16};
    SDL_RenderFillRect(g_app.renderer, &hdr2);
    rogue_font_draw_text(panel.x + 6, panel.y + 4, "STATS", 1, (RogueColor){255, 255, 255, 255});

    const char* labels[6] = {"STR", "DEX", "VIT", "INT", "CRIT%", "CRITDMG"};
    int values[6] = {g_app.player.strength,     g_app.player.dexterity,   g_app.player.vitality,
                     g_app.player.intelligence, g_app.player.crit_chance, g_app.player.crit_damage};
    for (int i = 0; i < 6; i++)
    {
        int highlight = (i == g_app.stats_panel_index);
        char line[64];
        snprintf(line, sizeof line, "%s %3d%s", labels[i], values[i], highlight ? " *" : "");
        rogue_font_draw_text(
            panel.x + 10, panel.y + 22 + i * 18, line, 1,
            (RogueColor){highlight ? 255 : 200, highlight ? 255 : 255, highlight ? 160 : 255, 255});
        int barw = values[i];
        if (i == 5)
            barw = values[i] / 4;
        if (barw > 70)
            barw = 70;
        if (barw < 0)
            barw = 0;
        SDL_SetRenderDrawColor(g_app.renderer, 50, 60, 90, 255);
        SDL_Rect bg = {panel.x + 10, panel.y + 22 + i * 18 + 10, 72, 4};
        SDL_RenderFillRect(g_app.renderer, &bg);
        SDL_SetRenderDrawColor(g_app.renderer, highlight ? 255 : 140, highlight ? 200 : 140,
                               highlight ? 90 : 160, 255);
        SDL_Rect fg = {panel.x + 10, panel.y + 22 + i * 18 + 10, barw, 4};
        SDL_RenderFillRect(g_app.renderer, &fg);
    }

    char footer[96];
    snprintf(footer, sizeof footer, "PTS:%d  ENTER=+  TAB=Close", g_app.unspent_stat_points);
    rogue_font_draw_text(panel.x + 6, panel.y + panel.h - 14, footer, 1,
                         (RogueColor){180, 220, 255, 255});
#endif
}
