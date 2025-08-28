/* hud_buff_belt.c - UI Phase 6.3 implementation */
#include "hud_buff_belt.h"
#include "../../graphics/font.h"
#include "../app/app_state.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_hud_buff_belt_refresh(RogueHUDBuffBeltState* st, double now_ms)
{
    if (!st)
        return;
    st->count = 0;
    RogueBuff buffs[ROGUE_HUD_MAX_BUFF_ICONS];
    int n = rogue_buffs_snapshot(buffs, ROGUE_HUD_MAX_BUFF_ICONS, now_ms);
    /* Group by type, accumulate stacks, and keep the instance with the longest remaining time. */
    for (int i = 0; i < n; i++)
    {
        int t = buffs[i].type;
        float remaining = (float) (buffs[i].end_ms - now_ms);
        if (remaining < 0)
            remaining = 0;
        float total = (float) (buffs[i].end_ms - buffs[i].last_apply_ms);
        if (total < 1.0f)
            total = remaining > 0 ? remaining : 1.0f;
        int found = -1;
        for (int j = 0; j < st->count; j++)
        {
            if (st->icons[j].type == t)
            {
                found = j;
                break;
            }
        }
        if (found < 0)
        {
            int idx = st->count++;
            if (idx >= ROGUE_HUD_MAX_BUFF_ICONS)
            {
                st->count = ROGUE_HUD_MAX_BUFF_ICONS;
                break;
            }
            st->icons[idx].type = t;
            st->icons[idx].magnitude = buffs[i].magnitude;
            st->icons[idx].remaining_ms = remaining;
            st->icons[idx].total_ms = total;
            st->icons[idx].stacks = 1;
        }
        else
        {
            RogueHUDBuffIcon* ic = &st->icons[found];
            ic->stacks += 1;
            /* Prefer displaying the larger magnitude and the longer remaining duration. */
            if (buffs[i].magnitude > ic->magnitude)
                ic->magnitude = buffs[i].magnitude;
            if (remaining > ic->remaining_ms)
            {
                ic->remaining_ms = remaining;
                ic->total_ms = total;
            }
        }
    }
}

void rogue_hud_buff_belt_render(const RogueHUDBuffBeltState* st, int screen_w)
{
#ifndef ROGUE_HAVE_SDL
    (void) st;
    (void) screen_w; /* suppress unused warnings when SDL disabled */
#endif
#ifdef ROGUE_HAVE_SDL
    if (!st || !g_app.renderer)
        return;
    int icon_w = 22, icon_h = 22;
    int gap = 4;
    int x = (screen_w - (st->count * icon_w + (st->count - 1) * gap)) / 2;
    int y = 4;
    if (x < 4)
        x = 4;
    for (int i = 0; i < st->count; i++)
    {
        const RogueHUDBuffIcon* ic = &st->icons[i];
        float pct = ic->total_ms > 0 ? (ic->remaining_ms / ic->total_ms) : 0.0f;
        if (pct < 0)
            pct = 0;
        if (pct > 1)
            pct = 1;
        /* Background */
        SDL_SetRenderDrawColor(g_app.renderer, 30, 30, 50, 200);
        SDL_Rect bg = {x, y, icon_w, icon_h};
        SDL_RenderFillRect(g_app.renderer, &bg);
        /* Border color based on category/source */
        uint32_t cats = rogue_buffs_type_categories((RogueBuffType) ic->type);
        int cr = 90, cg = 90, cb = 140; /* default */
        if (cats & (ROGUE_BUFF_CCFLAG_STUN | ROGUE_BUFF_CCFLAG_ROOT | ROGUE_BUFF_CCFLAG_SLOW))
        { /* CC/debuffs: red */
            cr = 200;
            cg = 80;
            cb = 80;
        }
        else if (cats & ROGUE_BUFF_CAT_OFFENSIVE)
        {
            cr = 220;
            cg = 140;
            cb = 60;
        }
        else if (cats & ROGUE_BUFF_CAT_DEFENSIVE)
        {
            cr = 100;
            cg = 200;
            cb = 110;
        }
        else if (cats & ROGUE_BUFF_CAT_MOVEMENT)
        {
            cr = 80;
            cg = 160;
            cb = 230;
        }
        else if (cats & ROGUE_BUFF_CAT_UTILITY)
        {
            cr = 170;
            cg = 130;
            cb = 220;
        }
        SDL_SetRenderDrawColor(g_app.renderer, (Uint8) cr, (Uint8) cg, (Uint8) cb, 255);
        SDL_Rect br = {x - 1, y - 1, icon_w + 2, icon_h + 2};
        SDL_RenderDrawRect(g_app.renderer, &br);
        /* Duration mini-bar (top, proportional) */
        int mini_h = 2;
        int mini_w = (int) ((float) icon_w * pct);
        if (mini_w > 0)
        {
            SDL_SetRenderDrawColor(g_app.renderer, 230, 230, 255, 220);
            SDL_Rect mb = {x, y - 3, mini_w, mini_h};
            SDL_RenderFillRect(g_app.renderer, &mb);
        }
        /* Cooldown/remaining overlay (filled from bottom -> remaining) */
        int overlay_h = (int) (icon_h * (1.0f - pct));
        if (overlay_h > 0)
        {
            SDL_SetRenderDrawColor(g_app.renderer, 0, 0, 0, 140);
            SDL_Rect ov = {x, y, icon_w, overlay_h};
            SDL_RenderFillRect(g_app.renderer, &ov);
        }
        char txt[8];
        snprintf(txt, sizeof txt, "%d", ic->magnitude);
        rogue_font_draw_text(x + 4, y + 4, txt, 1, (RogueColor){220, 220, 255, 255});
        /* Stack badge in top-right if stacks > 1 */
        if (ic->stacks > 1)
        {
            char sbuf[6];
            snprintf(sbuf, sizeof sbuf, "x%d", ic->stacks);
            rogue_font_draw_text(x + icon_w - 12, y - 2, sbuf, 1, (RogueColor){255, 220, 160, 255});
        }
        x += icon_w + gap;
    }
#endif
}
