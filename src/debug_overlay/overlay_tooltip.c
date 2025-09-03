#include "overlay_tooltip.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../core/app/app_state.h"
#include "overlay_core.h"
#include "overlay_input.h"
#include "overlay_theme.h"
#include "widgets/overlay_widgets_internal.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <string.h>

/* Minimal tooltip system for overlay: hover-delay then show a small panel near cursor. */

static int g_hover_id = -1;
static float g_hover_t = 0.0f;
static int g_active = 0;
static int g_anchor_x = 0, g_anchor_y = 0;
static char g_text[512];
static int g_any_tracked_this_frame = 0;

static void tooltip_reset(void)
{
    g_hover_id = -1;
    g_hover_t = 0.0f;
    g_active = 0;
    g_anchor_x = g_anchor_y = 0;
    g_text[0] = '\0';
}

void overlay_tooltip_new_frame(void) { g_any_tracked_this_frame = 0; }

void overlay_tooltip_track(int id, int x, int y, int w, int h, const char* text)
{
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    if (!text || !text[0])
        return;
    const OverlayInputState* in = overlay_input_get();
    if (!in)
        return;
    int over = (in->mouse_x >= x && in->mouse_x < x + w && in->mouse_y >= y && in->mouse_y < y + h);
    if (over)
    {
        g_any_tracked_this_frame = 1;
        if (g_hover_id == id)
        {
            g_hover_t += overlay_last_dt();
        }
        else
        {
            g_hover_id = id;
            g_hover_t = 0.0f;
            g_active = 0;
        }
        g_anchor_x = in->mouse_x + 14;
        g_anchor_y = in->mouse_y + 18;
        if (!g_active && g_hover_t > 0.40f)
        {
            /* Activate tooltip */
            strncpy(g_text, text, sizeof g_text - 1);
            g_text[sizeof g_text - 1] = '\0';
            g_active = 1;
        }
    }
}

static int wrap_lines(const char* txt, int max_chars, const char* out_lines[], int max_lines)
{
    int count = 0;
    const char* p = txt;
    while (*p && count < max_lines)
    {
        int len = 0;
        int last_space = -1;
        while (p[len] && len < max_chars)
        {
            if (p[len] == ' ')
                last_space = len;
            if (p[len] == '\n')
            {
                last_space = len;
                break;
            }
            len++;
        }
        int take = len;
        if (p[len] && last_space > 0 && last_space < len)
            take = last_space + 1;
        out_lines[count++] = p;
        p += take;
        while (*p == ' ')
            p++;
        if (*(p - 1) == '\n')
        { /* skip explicit newline */
        }
    }
    return count;
}

void overlay_tooltip_render(void)
{
    /* If nothing tracked this frame, decay state quickly */
    if (!g_any_tracked_this_frame)
    {
        tooltip_reset();
        return;
    }
#ifdef ROGUE_HAVE_SDL
    if (!g_active || g_text[0] == '\0' || g_app.headless || !g_app.renderer)
        return;
    const OverlayTheme* th = overlay_theme_get();
    /* Naive wrapping: 56 chars/line approx at scale 1 */
    const int max_chars = 56;
    const char* lines[12] = {0};
    int n = wrap_lines(g_text, max_chars, lines, 12);
    if (n <= 0)
        return;
    /* Compute box width/height using approx 6 px/char, 14 px/line */
    int maxw = 0;
    for (int i = 0; i < n; ++i)
    {
        int lw = 0;
        const char* s = lines[i];
        while (s[lw] && s[lw] != '\n' && lw < max_chars)
            lw++;
        if (lw > maxw)
            maxw = lw;
    }
    int box_w = 12 + maxw * 6;
    if (box_w < 120)
        box_w = 120;
    if (box_w > 420)
        box_w = 420;
    int box_h = 8 + n * 14;
    int x = g_anchor_x;
    int y = g_anchor_y;
    if (x + box_w > g_app.viewport_w)
        x = g_app.viewport_w - box_w - 8;
    if (y + box_h > g_app.viewport_h)
        y = g_app.viewport_h - box_h - 8;
    SDL_Rect r = {x, y, box_w, box_h};
    SDL_SetRenderDrawColor(g_app.renderer, th->panel_bg.r, th->panel_bg.g, th->panel_bg.b,
                           th->panel_bg.a);
    SDL_RenderFillRect(g_app.renderer, &r);
    SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                           th->panel_border.b, th->panel_border.a);
    SDL_RenderDrawRect(g_app.renderer, &r);
    /* Text */
    for (int i = 0; i < n; ++i)
    {
        char buf[64];
        int k = 0;
        const char* s = lines[i];
        while (s[k] && s[k] != '\n' && k < (int) sizeof buf - 1)
        {
            buf[k] = s[k];
            k++;
        }
        buf[k] = '\0';
        rogue_font_draw_text(x + 6, y + 4 + i * 14, buf, 1,
                             (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
    }
#else
    (void) g_active;
    (void) g_text;
    (void) g_anchor_x;
    (void) g_anchor_y;
#endif
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
