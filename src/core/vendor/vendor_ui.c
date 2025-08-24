#include "vendor_ui.h"
#include "../../graphics/font.h"
#include "../app_state.h"
#include "../durability.h" /* durability bucket */
#include "../equipment/equipment.h"
#include "../inventory/inventory.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include "../stat_cache.h"
#include "economy.h"
#include "vendor.h"
#include <stdio.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_vendor_panel_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.show_vendor_panel)
        return;
    if (!g_app.renderer)
        return;
    SDL_Rect panel = {g_app.viewport_w - 320, 60, 300, 260};
    SDL_SetRenderDrawColor(g_app.renderer, 20, 20, 32, 240);
    SDL_RenderFillRect(g_app.renderer, &panel);
    SDL_SetRenderDrawColor(g_app.renderer, 90, 90, 120, 255);
    SDL_Rect br = {panel.x - 2, panel.y - 2, panel.w + 4, panel.h + 4};
    SDL_RenderDrawRect(g_app.renderer, &br);
    rogue_font_draw_text(panel.x + 6, panel.y + 4, "VENDOR", 1, (RogueColor){255, 255, 210, 255});
    int y = panel.y + 24;
    int count = rogue_vendor_item_count();
    for (int i = 0; i < count; i++)
    {
        const RogueVendorItem* vi = rogue_vendor_get(i);
        if (!vi)
            continue;
        const RogueItemDef* d = rogue_item_def_at(vi->def_index);
        if (!d)
            continue;
        int price = rogue_econ_buy_price(vi);
        char line[128];
        snprintf(line, sizeof line, "%c %s (%d) %dG", (i == g_app.vendor_selection) ? '>' : ' ',
                 d->name, vi->rarity, price);
        rogue_font_draw_text(panel.x + 10, y, line, 1,
                             (RogueColor){(i == g_app.vendor_selection) ? 255 : 200,
                                          (i == g_app.vendor_selection) ? 255 : 200,
                                          (i == g_app.vendor_selection) ? 160 : 200, 255});
        y += 18;
        if (y > panel.y + panel.h - 40)
            break; /* clip */
    }
    /* Restock timer bar (Phase 4.7) */
    double t = g_app.vendor_time_accum_ms;
    double interval = g_app.vendor_restock_interval_ms;
    if (interval <= 0.0)
        interval = 1.0;
    if (t < 0)
        t = 0;
    if (t > interval)
        t = interval;
    float frac = (float) (t / interval);
    int bar_w = panel.w - 12;
    int bar_x = panel.x + 6;
    int bar_y = panel.y + panel.h - 34;
    int bar_h = 8;
    SDL_SetRenderDrawColor(g_app.renderer, 40, 40, 60, 255);
    SDL_Rect bar_bg = {bar_x, bar_y, bar_w, bar_h};
    SDL_RenderFillRect(g_app.renderer, &bar_bg);
    SDL_SetRenderDrawColor(g_app.renderer, 80, 180, 255, 255);
    SDL_Rect bar_fg = {bar_x, bar_y, (int) (bar_w * frac), bar_h};
    SDL_RenderFillRect(g_app.renderer, &bar_fg);
    char eta[64];
    double remain = interval - t;
    int sec = (int) (remain / 1000.0);
    if (sec < 0)
        sec = 0;
    snprintf(eta, sizeof eta, "Restock:%ds", sec);
    rogue_font_draw_text(bar_x, bar_y - 14, eta, 1, (RogueColor){200, 230, 255, 255});
    char footer[96];
    snprintf(footer, sizeof footer, "Gold:%d  REP:%d  ENTER=Buy  V=Close", rogue_econ_gold(),
             rogue_econ_get_reputation());
    rogue_font_draw_text(panel.x + 6, panel.y + panel.h - 18, footer, 1,
                         (RogueColor){180, 220, 255, 255});

    /* Phase 4.8 Transaction confirmation modal */
    if (g_app.vendor_confirm_active)
    {
        SDL_Rect modal = {panel.x - 140, panel.y + 40, 130, 110};
        SDL_SetRenderDrawColor(g_app.renderer, 30, 30, 50, 245);
        SDL_RenderFillRect(g_app.renderer, &modal);
        SDL_SetRenderDrawColor(g_app.renderer, 120, 120, 180, 255);
        SDL_Rect mbr = {modal.x - 2, modal.y - 2, modal.w + 4, modal.h + 4};
        SDL_RenderDrawRect(g_app.renderer, &mbr);
        const RogueItemDef* d = rogue_item_def_at(g_app.vendor_confirm_def_index);
        char name[64];
        snprintf(name, sizeof name, "%s", d ? d->name : "Item");
        char price[64];
        snprintf(price, sizeof price, "Price:%dG", g_app.vendor_confirm_price);
        int afford = rogue_econ_gold() >= g_app.vendor_confirm_price;
        RogueColor name_col = {255, 255, 210, 255};
        RogueColor price_col =
            afford ? (RogueColor){180, 255, 180, 255} : (RogueColor){255, 140, 140, 255};
        rogue_font_draw_text(modal.x + 6, modal.y + 6, "Confirm", 1,
                             (RogueColor){200, 220, 255, 255});
        rogue_font_draw_text(modal.x + 6, modal.y + 24, name, 1, name_col);
        rogue_font_draw_text(modal.x + 6, modal.y + 40, price, 1, price_col);
        rogue_font_draw_text(modal.x + 6, modal.y + 58, "ENTER=Yes", 1,
                             (RogueColor){200, 240, 200, 255});
        rogue_font_draw_text(modal.x + 6, modal.y + 74, "ESC=No", 1,
                             (RogueColor){240, 200, 200, 255});
        if (!afford && g_app.vendor_insufficient_flash_ms > 0)
        {
            /* simple flashing overlay */
            int alpha_i =
                (int) (120 + 80 * (((int) (g_app.vendor_insufficient_flash_ms / 120.0)) % 2));
            if (alpha_i < 0)
                alpha_i = 0;
            if (alpha_i > 255)
                alpha_i = 255;
            Uint8 alpha = (Uint8) alpha_i;
            SDL_SetRenderDrawColor(g_app.renderer, 255, 60, 60, alpha);
            SDL_RenderFillRect(g_app.renderer, &modal);
        }
    }
#endif
}

float rogue_vendor_restock_fraction(void)
{
    double t = g_app.vendor_time_accum_ms;
    double interval = g_app.vendor_restock_interval_ms;
    if (interval <= 0.0)
        return 0.0f;
    if (t < 0)
        t = 0;
    if (t > interval)
        t = interval;
    return (float) (t / interval);
}

/* Durability bucket helper moved to durability.c (Phase 8). */

void rogue_equipment_panel_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.show_equipment_panel)
        return;
    if (!g_app.renderer)
        return;
    SDL_Rect panel = {g_app.viewport_w - 320, 330, 300, 160};
    SDL_SetRenderDrawColor(g_app.renderer, 28, 18, 18, 235);
    SDL_RenderFillRect(g_app.renderer, &panel);
    SDL_SetRenderDrawColor(g_app.renderer, 120, 60, 60, 255);
    SDL_Rect br = {panel.x - 2, panel.y - 2, panel.w + 4, panel.h + 4};
    SDL_RenderDrawRect(g_app.renderer, &br);
    rogue_font_draw_text(panel.x + 6, panel.y + 4, "EQUIPMENT", 1,
                         (RogueColor){255, 230, 230, 255});
    rogue_font_draw_text(panel.x + 10, panel.y + 26, "Weapon Slot: (W)", 1,
                         (RogueColor){220, 200, 200, 255});
    rogue_font_draw_text(panel.x + 10, panel.y + 44, "Armor Slot : (A)", 1,
                         (RogueColor){200, 220, 200, 255});
    char stats[96];
    snprintf(stats, sizeof stats, "STR:%d DEX:%d VIT:%d INT:%d", g_app.player.strength,
             g_app.player.dexterity, g_app.player.vitality, g_app.player.intelligence);
    rogue_font_draw_text(panel.x + 10, panel.y + panel.h - 56, stats, 1,
                         (RogueColor){255, 255, 180, 255});
    int w_inst = rogue_equip_get(ROGUE_EQUIP_WEAPON);
    int cur = 0, max = 0;
    if (w_inst >= 0)
        rogue_item_instance_get_durability(w_inst, &cur, &max);
    if (max > 0)
    {
        float pct = (max > 0) ? (float) cur / (float) max : 0.0f;
        int bucket = rogue_durability_bucket(pct);
        char dur[64];
        snprintf(dur, sizeof dur, "WEAPON DUR:%d/%d (R=Repair)", cur, max);
        RogueColor txt_col = bucket == 2 ? (RogueColor){200, 255, 200, 255}
                                         : (bucket == 1 ? (RogueColor){255, 210, 120, 255}
                                                        : (RogueColor){255, 120, 120, 255});
        rogue_font_draw_text(panel.x + 10, panel.y + panel.h - 40, dur, 1, txt_col);
        /* Draw durability bar */
        int bx = panel.x + 10;
        int by = panel.y + panel.h - 52;
        int bw = panel.w - 20;
        int bh = 6;
        SDL_SetRenderDrawColor(g_app.renderer, 40, 40, 40, 255);
        SDL_Rect bg = {bx, by, bw, bh};
        SDL_RenderFillRect(g_app.renderer, &bg);
        int fillw = (int) (bw * pct);
        if (fillw < 0)
            fillw = 0;
        if (fillw > bw)
            fillw = bw;
        Uint8 r = 80, g = 200, b = 80;
        if (bucket == 1)
        {
            r = 230;
            g = 170;
            b = 40;
        }
        else if (bucket == 0)
        {
            r = 220;
            g = 50;
            b = 50;
        }
        SDL_SetRenderDrawColor(g_app.renderer, r, g, b, 255);
        SDL_Rect fg = {bx, by, fillw, bh};
        SDL_RenderFillRect(g_app.renderer, &fg);
        if (bucket == 0)
        {
            rogue_font_draw_text(bx + bw - 14, by - 6, "!", 1, (RogueColor){255, 80, 80, 255});
        }
    }
    char derived[96];
    snprintf(derived, sizeof derived, "DPS:%d EHP:%d Gold:%d", g_player_stat_cache.dps_estimate,
             g_player_stat_cache.ehp_estimate, rogue_econ_gold());
    rogue_font_draw_text(panel.x + 10, panel.y + panel.h - 22, derived, 1,
                         (RogueColor){200, 240, 200, 255});
#endif
}
