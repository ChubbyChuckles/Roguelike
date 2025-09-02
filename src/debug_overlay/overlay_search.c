#include "overlay_search.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../core/app/app_state.h"
#include "../core/loot/loot_item_defs.h"
#include "../core/skills/skill_debug.h"
#include "overlay_core.h"
#include "overlay_input.h"
#include "overlay_theme.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <stdio.h>
#include <string.h>

typedef struct SearchHit
{
    int kind;          /* 0=item, 1=skill */
    int index;         /* item index or skill id */
    const char* label; /* temporary pointer for draw */
} SearchHit;

static int g_open = 0;
static char g_q[64];
static int g_sel = 0;

static int ci_contains(const char* hay, const char* needle)
{
    if (!needle || !needle[0])
        return 1;
    if (!hay)
        return 0;
    size_t n = strlen(needle);
    for (const char* p = hay; *p; ++p)
    {
        size_t i = 0;
        while (i < n && p[i] &&
               (char) tolower((unsigned char) p[i]) == (char) tolower((unsigned char) needle[i]))
            ++i;
        if (i == n)
            return 1;
    }
    return 0;
}

void overlay_search_toggle(int open)
{
    g_open = open ? 1 : 0;
    if (g_open)
    {
        g_q[0] = '\0';
        g_sel = 0;
    }
}

void overlay_search_render(void)
{
    if (!overlay_is_enabled() || !g_open)
        return;
    const OverlayTheme* th = overlay_theme_get();
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
#endif
    int w = (int) (g_app.viewport_w * 0.5f);
    int x = (g_app.viewport_w - w) / 2;
    int y = (int) (g_app.viewport_h * 0.10f);
    int h = 28 + 6 + 10 * 20 + 8; /* input + spacing + 10 rows */
#ifdef ROGUE_HAVE_SDL
    SDL_Rect r = {x, y, w, h};
    SDL_SetRenderDrawColor(g_app.renderer, th->panel_bg.r, th->panel_bg.g, th->panel_bg.b,
                           th->panel_bg.a);
    SDL_RenderFillRect(g_app.renderer, &r);
    SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                           th->panel_border.b, th->panel_border.a);
    SDL_RenderDrawRect(g_app.renderer, &r);
#endif
    rogue_font_draw_text(
        x + 8, y + 6, "/", 1,
        (RogueColor){th->text_accent.r, th->text_accent.g, th->text_accent.b, th->text_accent.a});

    const OverlayInputState* in = overlay_input_get();
    if (in->text_input[0])
    {
        size_t cur = strlen(g_q);
        size_t avail = sizeof g_q - 1 - cur;
        strncat(g_q, in->text_input, avail);
    }
    if (in->key_backspace_pressed)
    {
        size_t l = strlen(g_q);
        if (l > 0)
            g_q[l - 1] = '\0';
    }
    rogue_font_draw_text(x + 22, y + 6, g_q, 1,
                         (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});

    SearchHit hits[32];
    int hc = 0;
    /* Items */
    int icount = rogue_item_defs_count();
    for (int i = 0; i < icount && hc < (int) (sizeof hits / sizeof hits[0]); ++i)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (!d)
            continue;
        if (!ci_contains(d->name, g_q) && !ci_contains(d->id, g_q))
            continue;
        hits[hc++] = (SearchHit){0, i, d->name ? d->name : d->id};
    }
    /* Skills */
    int scount = rogue_skill_debug_count();
    for (int i = 0; i < scount && hc < (int) (sizeof hits / sizeof hits[0]); ++i)
    {
        const char* nm = rogue_skill_debug_name(i);
        if (!ci_contains(nm, g_q))
            continue;
        hits[hc++] = (SearchHit){1, i, nm};
    }

    if (g_sel < 0)
        g_sel = 0;
    if (g_sel >= hc)
        g_sel = hc - 1;

    int dy = y + 28 + 6;
    for (int i = 0; i < hc && i < 10; ++i)
    {
#ifdef ROGUE_HAVE_SDL
        SDL_Rect row = {x + 6, dy - 2, w - 12, 20};
        SDL_SetRenderDrawColor(g_app.renderer,
                               (i == g_sel) ? th->table_row_bg_sel.r : th->table_row_bg.r,
                               (i == g_sel) ? th->table_row_bg_sel.g : th->table_row_bg.g,
                               (i == g_sel) ? th->table_row_bg_sel.b : th->table_row_bg.b,
                               (i == g_sel) ? th->table_row_bg_sel.a : th->table_row_bg.a);
        SDL_RenderFillRect(g_app.renderer, &row);
        SDL_SetRenderDrawColor(g_app.renderer, th->table_border.r, th->table_border.g,
                               th->table_border.b, th->table_border.a);
        SDL_RenderDrawRect(g_app.renderer, &row);
#endif
        char lbl[256];
        snprintf(lbl, sizeof lbl, "%s  [%s]", hits[i].label ? hits[i].label : "?",
                 hits[i].kind == 0 ? "Item" : "Skill");
        rogue_font_draw_text(
            x + 12, dy, lbl, 1,
            (RogueColor){th->table_text.r, th->table_text.g, th->table_text.b, th->table_text.a});
        dy += 22;
    }

    /* navigation */
    if (in->key_down_pressed)
        g_sel++;
    if (in->key_up_pressed)
        g_sel--;
    if (g_sel < 0)
        g_sel = 0;
    if (g_sel >= hc)
        g_sel = hc - 1;
    if (in->key_enter_pressed && hc > 0 && g_sel >= 0 && g_sel < hc)
    {
        if (hits[g_sel].kind == 0)
            overlay_nav_open_items_and_select(hits[g_sel].index);
        else
            overlay_nav_open_skills_and_select(hits[g_sel].index);
        overlay_search_toggle(0);
    }
    if (in->key_escape_pressed)
        overlay_search_toggle(0);
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
