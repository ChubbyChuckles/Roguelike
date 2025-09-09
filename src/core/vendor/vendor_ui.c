#include "vendor_ui.h"
#include "../../game/durability.h" /* durability bucket */
#include "../../game/stat_cache.h"
#include "../../graphics/font.h"
#include "../app/app_state.h"
#include "../equipment/equipment.h"
#include "../inventory/inventory.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include "economy.h"
#include "vendor.h"
#include "vendor_pricing.h"
#include "vendor_registry.h"
#include "vendor_reputation.h"
#include "vendor_security.h"
#include "vendor_ui_filters.h"
#include "vendor_ui_negotiation.h"
#include <stdio.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

int rogue_vendor_tab_get(void) { return g_app.vendor_tab; }
void rogue_vendor_tab_set(int tab)
{
    if (tab < 0)
        tab = 0;
    if (tab > 3)
        tab = 3;
    g_app.vendor_tab = tab;
}

/* Phase 16.6 accessibility: text-only toggle (module-local) */
static int g_vendor_text_only = 0;
void rogue_vendor_ui_set_text_only(int enabled) { g_vendor_text_only = enabled ? 1 : 0; }
int rogue_vendor_ui_text_only(void) { return g_vendor_text_only; }

/* Phase 16.6: high-contrast delta/text-only friendly line formatter */
int rogue_vendor_ui_format_price_line(const char* item_name, int rarity, int price, int last_price,
                                      int selected, int high_contrast, char* out, int cap)
{
    if (!out || cap <= 0)
        return 0;
    int wrote = 0;
    const char sel = selected ? '>' : ' ';
    int delta = (last_price > 0) ? (price - last_price) : 0;
    /* In text-only, avoid color semantics; include explicit delta suffix. */
    if (g_vendor_text_only)
    {
        if (last_price > 0)
        {
            wrote = snprintf(out, cap, "%c %s (%d) %dG [was %dG, %s%d%%]", sel,
                             item_name ? item_name : "Item", rarity, price, last_price,
                             (delta >= 0 ? "+" : ""),
                             (last_price > 0 ? (delta * 100) / (last_price ? last_price : 1) : 0));
        }
        else
        {
            wrote = snprintf(out, cap, "%c %s (%d) %dG", sel, item_name ? item_name : "Item",
                             rarity, price);
        }
        return wrote;
    }
    /* Default rich text: same content; when high_contrast include explicit delta suffix. */
    if (high_contrast && last_price > 0)
    {
        int pct = (delta * 100) / (last_price ? last_price : 1);
        wrote = snprintf(out, cap, "%c %s (%d) %dG  %s%d%%", sel, item_name ? item_name : "Item",
                         rarity, price, (delta >= 0 ? "+" : ""), pct);
    }
    else
    {
        wrote = snprintf(out, cap, "%c %s (%d) %dG", sel, item_name ? item_name : "Item", rarity,
                         price);
    }
    return wrote;
}

static void draw_tabs(SDL_Rect panel)
{
    const char* labels[4] = {"BUY", "SELL", "BUYBACK", "SPECIAL"};
    int x = panel.x + 6;
    int y = panel.y + 20;
    for (int i = 0; i < 4; i++)
    {
        RogueColor col = (i == g_app.vendor_tab) ? (RogueColor){255, 235, 180, 255}
                                                 : (RogueColor){190, 190, 210, 255};
        rogue_font_draw_text(x, y, labels[i], 1, col);
        x += 72;
    }
}

static int passes_filters(const RogueItemDef* d, int rarity, const RogueVendorUiFilters* f)
{
    if (!d || !f)
        return 1;
    /* Category mask: if bit for category is 0, hide */
    if (f->category_mask != -1)
    {
        int bit = (d->category >= 0 && d->category < 32) ? (1 << d->category) : 0;
        if ((f->category_mask & bit) == 0)
            return 0;
    }
    if (rarity < f->min_rarity || rarity > f->max_rarity)
        return 0;
    /* stat_min/max: placeholder check on base_value as proxy */
    if (d->base_value < f->stat_min || d->base_value > f->stat_max)
        return 0;
    return 1;
}

static void draw_buy_list(SDL_Rect panel)
{
    int y = panel.y + 44;
    RogueVendorUiFilters f = rogue_vendor_ui_get_filters();
    int count = rogue_vendor_item_count();
    for (int i = 0; i < count; i++)
    {
        const RogueVendorItem* vi = rogue_vendor_get(i);
        if (!vi)
            continue;
        const RogueItemDef* d = rogue_item_def_at(vi->def_index);
        if (!d || !passes_filters(d, vi->rarity, &f))
            continue;
        int price = rogue_econ_buy_price(vi);
        /* Accessibility: compute last price via breakdown for delta if possible. */
        int last_price = 0;
        {
            RogueVendorPriceBreakdown br;
            (void) rogue_vendor_compute_price_with_breakdown(
                g_app.vendor_def_index, vi->def_index, vi->rarity, (int) (d ? d->category : 0),
                1, /* vendor selling */
                100, /* assume pristine */ -1, 0.0f, &br);
            last_price = br.final_price; /* snapshot of current factors; future: cache prev */
        }
        char line[192];
        rogue_vendor_ui_format_price_line(d->name, vi->rarity, price, last_price,
                                          (i == g_app.vendor_selection), g_app.high_contrast, line,
                                          (int) sizeof line);
        RogueColor col = {(i == g_app.vendor_selection) ? 255 : 200,
                          (i == g_app.vendor_selection) ? 255 : 200,
                          (i == g_app.vendor_selection) ? 160 : 200, 255};
        if (g_vendor_text_only)
        {
            /* Force bright white for readability in text-only. */
            col.r = col.g = col.b = 240;
        }
        rogue_font_draw_text(panel.x + 10, y, line, 1, col);
        y += 18;
        if (y > panel.y + panel.h - 64)
            break;
    }
    /* Tooltip for selected item (pricing breakdown) */
    const RogueVendorItem* sel = rogue_vendor_get(g_app.vendor_selection);
    if (sel)
    {
        RogueVendorPriceBreakdown br;
        (void) rogue_vendor_compute_price_with_breakdown(
            g_app.vendor_def_index, sel->def_index, sel->rarity,
            (int) (rogue_item_def_at(sel->def_index) ? rogue_item_def_at(sel->def_index)->category
                                                     : 0),
            1, /* vendor selling */ 100, /* assume pristine in UI list */ -1, 0.0f, &br);
        char tip[256];
        rogue_vendor_format_price_tooltip(&br, tip, (int) sizeof tip);
        rogue_font_draw_text(panel.x + 10, panel.y + panel.h - 56, tip, 1,
                             (RogueColor){180, 220, 255, 255});
    }
}

void rogue_vendor_panel_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.show_vendor_panel)
        return;
    if (!g_app.renderer)
        return;
    SDL_Rect panel = {g_app.viewport_w - 320, 60, 300, 300};
    SDL_SetRenderDrawColor(g_app.renderer, g_app.high_contrast ? 10 : 20,
                           g_app.high_contrast ? 10 : 20, g_app.high_contrast ? 10 : 32, 240);
    SDL_RenderFillRect(g_app.renderer, &panel);
    SDL_SetRenderDrawColor(g_app.renderer, g_app.high_contrast ? 255 : 90,
                           g_app.high_contrast ? 255 : 90, g_app.high_contrast ? 255 : 120, 255);
    SDL_Rect br = {panel.x - 2, panel.y - 2, panel.w + 4, panel.h + 4};
    SDL_RenderDrawRect(g_app.renderer, &br);
    rogue_font_draw_text(panel.x + 6, panel.y + 4, "VENDOR", 1,
                         (RogueColor){255, 255, g_app.high_contrast ? 255 : 210, 255});
    draw_tabs(panel);
    /* Phase 16.4: Reputation progress bar & tier label */
    int vdef = g_app.vendor_def_index;
    int tier = rogue_vendor_rep_current_tier(vdef);
    float repf = rogue_vendor_rep_progress(vdef);
    if (repf < 0.0f)
        repf = 0.0f;
    if (repf > 1.0f)
        repf = 1.0f;
    SDL_SetRenderDrawColor(g_app.renderer, g_app.high_contrast ? 20 : 36,
                           g_app.high_contrast ? 20 : 36, g_app.high_contrast ? 20 : 54, 255);
    SDL_Rect rep_bg = {panel.x + 8, panel.y + 28, panel.w - 16, 6};
    SDL_RenderFillRect(g_app.renderer, &rep_bg);
    SDL_SetRenderDrawColor(g_app.renderer, g_app.high_contrast ? 255 : 120,
                           g_app.high_contrast ? 255 : 210, g_app.high_contrast ? 255 : 140, 255);
    SDL_Rect rep_fg = {panel.x + 8, panel.y + 28, (int) ((panel.w - 16) * repf), 6};
    SDL_RenderFillRect(g_app.renderer, &rep_fg);
    char rep_label[96];
    const RogueRepTier* rtt = (tier >= 0) ? rogue_rep_tier_at(tier) : NULL;
    snprintf(rep_label, sizeof rep_label, "Rep: %s%s", rtt ? rtt->id : "None",
             (rtt && rtt->unlock_tags[0]) ? " (perks)" : "");
    rogue_font_draw_text(panel.x + 10, panel.y + 16, rep_label, 1,
                         (RogueColor){g_app.high_contrast ? 255 : 200,
                                      g_app.high_contrast ? 255 : 240,
                                      g_app.high_contrast ? 255 : 210, 255});
    /* Draw content based on tab */
    switch (g_app.vendor_tab)
    {
    case ROGUE_VENDOR_TAB_BUY:
        draw_buy_list(panel);
        /* Phase 16.3: Negotiation UI preview (bottom-left) */
        {
            int minp = 0, maxp = 0;
            float prob = 0.0f;
            (void) rogue_vendor_ui_negotiation_preview(
                NULL, g_app.player.strength, g_app.player.dexterity, g_app.player.intelligence,
                g_app.player.vitality, &minp, &maxp, &prob);
            char nego[96];
            unsigned int now_ms = (unsigned int) g_app.game_time_ms;
            int allowed = rogue_vendor_security_negotiation_allowed(vdef, now_ms);
            if (!allowed)
            {
                unsigned int rem =
                    rogue_vendor_security_negotiation_lockout_remaining(vdef, now_ms);
                snprintf(nego, sizeof nego, "Negotiation locked: %ums", rem);
                rogue_font_draw_text(panel.x + 10, panel.y + panel.h - 72, nego, 1,
                                     (RogueColor){g_app.high_contrast ? 255 : 255,
                                                  g_app.high_contrast ? 255 : 170,
                                                  g_app.high_contrast ? 255 : 150, 255});
            }
            else
            {
                snprintf(nego, sizeof nego, "Negotiate: %+d..%+d%% (p=%.0f%%)", -maxp, -minp,
                         prob * 100.0f);
                rogue_font_draw_text(panel.x + 10, panel.y + panel.h - 72, nego, 1,
                                     (RogueColor){g_app.high_contrast ? 255 : 210,
                                                  g_app.high_contrast ? 255 : 230,
                                                  g_app.high_contrast ? 255 : 255, 255});
            }
        }
        break;
    case ROGUE_VENDOR_TAB_SELL:
        rogue_font_draw_text(panel.x + 10, panel.y + 44, "Sell: select from inventory", 1,
                             (RogueColor){200, 220, 255, 255});
        break;
    case ROGUE_VENDOR_TAB_BUYBACK:
        rogue_font_draw_text(panel.x + 10, panel.y + 44, "Buyback: recent sold items", 1,
                             (RogueColor){200, 220, 255, 255});
        break;
    case ROGUE_VENDOR_TAB_SPECIAL:
        rogue_font_draw_text(panel.x + 10, panel.y + 44, "Special offers available!", 1,
                             (RogueColor){200, 220, 255, 255});
        break;
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
    SDL_SetRenderDrawColor(g_app.renderer, g_app.high_contrast ? 10 : 40,
                           g_app.high_contrast ? 10 : 40, g_app.high_contrast ? 10 : 60, 255);
    SDL_Rect bar_bg = {bar_x, bar_y, bar_w, bar_h};
    SDL_RenderFillRect(g_app.renderer, &bar_bg);
    SDL_SetRenderDrawColor(g_app.renderer, g_app.high_contrast ? 255 : 80,
                           g_app.high_contrast ? 255 : 180, 255, 255);
    SDL_Rect bar_fg = {bar_x, bar_y, (int) (bar_w * frac), bar_h};
    SDL_RenderFillRect(g_app.renderer, &bar_fg);
    char eta[64];
    double remain = interval - t;
    int sec = (int) (remain / 1000.0);
    if (sec < 0)
        sec = 0;
    snprintf(eta, sizeof eta, "Restock:%ds", sec);
    rogue_font_draw_text(bar_x, bar_y - 14, eta, 1, (RogueColor){200, 230, 255, 255});
    char footer[128];
    snprintf(footer, sizeof footer, "Gold:%d  REP:%d  TAB:%d  \x1B[<] [>] tabs  ENTER=Buy  V=Close",
             rogue_econ_gold(), rogue_econ_get_reputation(), g_app.vendor_tab);
    rogue_font_draw_text(
        panel.x + 6, panel.y + panel.h - 18, footer, 1,
        (RogueColor){g_app.high_contrast ? 255 : 180, g_app.high_contrast ? 255 : 220, 255, 255});

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
