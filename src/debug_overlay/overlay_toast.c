#include "overlay_toast.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../core/app/app_state.h"
#include "../graphics/font.h"
#include "overlay_core.h"
#include "overlay_theme.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <string.h>

typedef struct Toast
{
    int active;
    enum OverlayToastKind kind;
    char msg[192];
    int ttl_ms; /* time-to-live remaining */
} Toast;

#define TOAST_MAX 8
static Toast g_toasts[TOAST_MAX];

void overlay_toast_push(enum OverlayToastKind kind, const char* msg, int duration_ms)
{
    if (!msg || duration_ms <= 0)
        return;
    /* find slot */
    int idx = -1;
    for (int i = 0; i < TOAST_MAX; ++i)
        if (!g_toasts[i].active)
        {
            idx = i;
            break;
        }
    if (idx < 0)
        idx = 0; /* overwrite oldest */
    g_toasts[idx].active = 1;
    g_toasts[idx].kind = kind;
    strncpy(g_toasts[idx].msg, msg, sizeof g_toasts[idx].msg - 1);
    g_toasts[idx].msg[sizeof g_toasts[idx].msg - 1] = '\0';
    g_toasts[idx].ttl_ms = duration_ms;
}

void overlay_toast_clear(void) { memset(g_toasts, 0, sizeof g_toasts); }

void overlay_toast_render(void)
{
    if (!overlay_is_enabled())
        return;
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
#endif
    const OverlayTheme* th = overlay_theme_get();
    int x = g_app.viewport_w - 10; /* right padding */
    int y = 10;                    /* top padding */
    int pad = 6;
    int line_h = (int) (th->font_size * th->dpi_scale) + 6;
    int used = 0;
    for (int i = 0; i < TOAST_MAX; ++i)
    {
        if (!g_toasts[i].active)
            continue;
        /* measure text roughly by characters; font has fixed advance in our renderer */
        int chars = (int) strlen(g_toasts[i].msg);
        int w = (int) (chars * (th->font_size * 0.55f) * th->dpi_scale) + pad * 2;
        int h = line_h + pad * 2;
        int px = x - w; /* right align */
        int py = y;
#ifdef ROGUE_HAVE_SDL
        SDL_Rect r = {px, py, w, h};
        OverlayColor bg = th->toast_info_bg;
        if (g_toasts[i].kind == OVERLAY_TOAST_WARN)
            bg = th->toast_warn_bg;
        else if (g_toasts[i].kind == OVERLAY_TOAST_ERROR)
            bg = th->toast_error_bg;
        SDL_SetRenderDrawColor(g_app.renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(g_app.renderer, &r);
        SDL_SetRenderDrawColor(g_app.renderer, th->table_border.r, th->table_border.g,
                               th->table_border.b, th->table_border.a);
        SDL_RenderDrawRect(g_app.renderer, &r);
#endif
        rogue_font_draw_text(
            px + pad, py + pad, g_toasts[i].msg, 1,
            (RogueColor){th->toast_text.r, th->toast_text.g, th->toast_text.b, th->toast_text.a});
        y += h + 6;
        used++;
        /* advance timers */
        int dt_ms = (int) (overlay_last_dt() * 1000.0f);
        if (dt_ms < 0)
            dt_ms = 0;
        g_toasts[i].ttl_ms -= dt_ms;
        if (g_toasts[i].ttl_ms <= 0)
            g_toasts[i].active = 0;
    }
    (void) used;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
