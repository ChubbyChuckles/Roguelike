#include "overlay_core.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../content/json_io.h"
#include "overlay_commands.h"
#include "overlay_input.h"
#include "overlay_theme.h"
#include "overlay_toast.h"
#include "widgets/overlay_widgets_internal.h"
#include <stdio.h>
#include <string.h>

/* Ensure we can toggle SDL blend mode while rendering overlay backdrops */
#include "../core/app/app_state.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#define OVERLAY_MAX_PANELS 32
static OverlayPanel g_panels[OVERLAY_MAX_PANELS];
static int g_panel_count = 0;
static unsigned int g_panel_visible_mask = 0u; /* bit i = 1 -> panel i visible */
static int g_enabled = 0;
static float g_last_dt = 0.0f;
static int g_last_w = 0;
static int g_last_h = 0;

typedef struct PanelLayout
{
    char id[64];
    int x, y, w;
    int visible;
} PanelLayout;

#define OVERLAY_MAX_LAYOUTS 32
static PanelLayout g_layouts[OVERLAY_MAX_LAYOUTS];
static int g_layout_count = 0;

static const char* overlay_layout_path(void)
{
    return "build/overlay_layout.json"; /* inside build dir alongside overrides */
}

static int overlay_layout_find(const char* id)
{
    for (int i = 0; i < g_layout_count; ++i)
        if (strcmp(g_layouts[i].id, id) == 0)
            return i;
    return -1;
}

static void overlay_layout_set(const char* id, int x, int y, int w, int visible)
{
    int i = overlay_layout_find(id);
    if (i < 0 && g_layout_count < OVERLAY_MAX_LAYOUTS)
    {
        i = g_layout_count++;
        memset(&g_layouts[i], 0, sizeof g_layouts[i]);
        strncpy(g_layouts[i].id, id ? id : "", sizeof g_layouts[i].id - 1);
    }
    if (i >= 0)
    {
        g_layouts[i].x = x;
        g_layouts[i].y = y;
        g_layouts[i].w = w;
        g_layouts[i].visible = visible ? 1 : 0;
    }
}

static int overlay_layout_get(const char* id, int* x, int* y, int* w, int* visible)
{
    int i = overlay_layout_find(id);
    if (i < 0)
        return -1;
    if (x)
        *x = g_layouts[i].x;
    if (y)
        *y = g_layouts[i].y;
    if (w)
        *w = g_layouts[i].w;
    if (visible)
        *visible = g_layouts[i].visible;
    return 0;
}

static void overlay_layout_save(void)
{
    char buf[4096];
    int n = 0;
    n += snprintf(buf + n, sizeof buf - n, "{\n  \"panels\": [\n");
    for (int i = 0; i < g_layout_count; ++i)
    {
        n += snprintf(buf + n, sizeof buf - n,
                      "    {\"id\": \"%s\", \"x\": %d, \"y\": %d, \"w\": %d, \"visible\": %d}%s\n",
                      g_layouts[i].id, g_layouts[i].x, g_layouts[i].y, g_layouts[i].w,
                      g_layouts[i].visible, (i + 1 < g_layout_count) ? "," : "");
    }
    n += snprintf(buf + n, sizeof buf - n, "  ]\n}\n");
    char err[256];
    (void) json_io_write_atomic(overlay_layout_path(), buf, (size_t) n, err, (int) sizeof err);
}

static void overlay_layout_load(void)
{
    char* data = NULL;
    size_t len = 0;
    char err[256];
    if (json_io_read_file(overlay_layout_path(), &data, &len, err, (int) sizeof err) != 0 || !data)
        return;
    /* Very small hand-rolled parse: look for entries of form
     * {"id":"...","x":N,"y":N,"w":N,"visible":B} */
    const char* p = data;
    while ((p = strstr(p, "\"id\"")) != NULL)
    {
        p = strchr(p, '"');
        if (!p)
            break;
        p = strchr(p + 1, '"');
        if (!p)
            break;
        if (*p != '"')
            break;
        /* find id value */
        const char* id_key = strstr(p, ":");
        if (!id_key)
            break;
        const char* id_start = strchr(id_key, '"');
        if (!id_start)
            break;
        id_start++;
        const char* id_end = strchr(id_start, '"');
        if (!id_end)
            break;
        char id[64] = {0};
        size_t id_len = (size_t) (id_end - id_start);
        if (id_len >= sizeof id)
            id_len = sizeof id - 1;
        memcpy(id, id_start, id_len);
        /* find x,y,w,visible after this */
        int x = 10, y = 10, w = 360, vis = 1;
        const char* q = id_end;
        sscanf(q, "%*[^x]x%*[^0-9-]%d%*[^y]y%*[^0-9-]%d%*[^w]w%*[^0-9-]%d%*[^v]visible%*[^0-9]%d",
               &x, &y, &w, &vis);
        overlay_layout_set(id, x, y, w, vis);
        p = id_end + 1;
    }
    free(data);
}

void overlay_init(void)
{
    g_panel_count = 0;
    g_enabled = 0;
    g_panel_visible_mask = 0u;
    g_layout_count = 0;
    overlay_layout_load();
    /* Initialize theme after layout so future theme UI can use persisted data */
    overlay_theme_init();
}

void overlay_shutdown(void)
{
    g_panel_count = 0;
    g_enabled = 0;
}

int overlay_register_panel(const char* id, const char* name, void (*fn)(void*), void* user)
{
    if (g_panel_count >= OVERLAY_MAX_PANELS)
        return -1;
    g_panels[g_panel_count].id = id;
    g_panels[g_panel_count].name = name;
    g_panels[g_panel_count].fn = fn;
    g_panels[g_panel_count].user = user;
    g_panel_count++;
    /* Default newly-registered panels to visible */
    g_panel_visible_mask |= (1u << (g_panel_count - 1));
    /* Merge persisted visibility if present */
    int vis = 0;
    if (overlay_layout_get(id, NULL, NULL, NULL, &vis) == 0)
    {
        if (vis)
            g_panel_visible_mask |= (1u << (g_panel_count - 1));
        else
            g_panel_visible_mask &= ~(1u << (g_panel_count - 1));
    }
    return g_panel_count - 1;
}

void overlay_new_frame(float dt, int screen_w, int screen_h)
{
    (void) dt;
    (void) screen_w;
    (void) screen_h;
    g_last_dt = dt;
    g_last_w = screen_w;
    g_last_h = screen_h;
}

void overlay_render(void)
{
    if (!g_enabled)
        return;
        /* Enable alpha blending so translucent panel backgrounds are visible */
#ifdef ROGUE_HAVE_SDL
    SDL_BlendMode prev_mode = SDL_BLENDMODE_NONE;
    int restore_blend = 0;
    if (!g_app.headless && g_app.renderer)
    {
        if (SDL_GetRenderDrawBlendMode(g_app.renderer, &prev_mode) == 0)
            restore_blend = 1;
        SDL_SetRenderDrawBlendMode(g_app.renderer, SDL_BLENDMODE_BLEND);
    }
#endif
    /* Global shortcuts */
    {
        const OverlayInputState* in = overlay_input_get();
        if (in && in->key_p_pressed && in->key_ctrl_down && in->key_shift_down)
        {
            /* toggle command palette */
            overlay_commands_toggle(1);
        }
    }
    /* Invoke panel callbacks; panels draw using SDL primitives and fonts */
    for (int i = 0; i < g_panel_count; ++i)
    {
        if (g_panels[i].fn && (g_panel_visible_mask & (1u << i)))
            g_panels[i].fn(g_panels[i].user);
    }
    /* Non-modal systems */
    overlay_toast_render();
    overlay_commands_render();
    /* Restore previous blend mode to avoid side effects on game renderer */
#ifdef ROGUE_HAVE_SDL
    if (restore_blend && g_app.renderer)
        SDL_SetRenderDrawBlendMode(g_app.renderer, prev_mode);
#endif
    (void) g_last_dt;
    (void) g_last_w;
    (void) g_last_h;
}

void overlay_set_enabled(int enabled) { g_enabled = enabled ? 1 : 0; }

int overlay_is_enabled(void) { return g_enabled; }

float overlay_last_dt(void) { return g_last_dt; }

int overlay_get_panel_count(void) { return g_panel_count; }

int overlay_get_panel_info(int index, const char** out_id, const char** out_name, int* out_visible)
{
    if (index < 0 || index >= g_panel_count)
        return -1;
    if (out_id)
        *out_id = g_panels[index].id;
    if (out_name)
        *out_name = g_panels[index].name;
    if (out_visible)
        *out_visible = (g_panel_visible_mask & (1u << index)) ? 1 : 0;
    return 0;
}

int overlay_set_panel_visible_by_index(int index, int visible)
{
    if (index < 0 || index >= g_panel_count)
        return -1;
    if (visible)
        g_panel_visible_mask |= (1u << index);
    else
        g_panel_visible_mask &= ~(1u << index);
    /* Persist change */
    if (index >= 0 && index < g_panel_count)
        overlay_layout_set(g_panels[index].id, 10, 10, 360, visible);
    overlay_layout_save();
    return 0;
}

int overlay_get_panel_visible_by_index(int index)
{
    if (index < 0 || index >= g_panel_count)
        return 0;
    return (g_panel_visible_mask & (1u << index)) ? 1 : 0;
}

static int overlay_find_panel_index(const char* id)
{
    if (!id)
        return -1;
    for (int i = 0; i < g_panel_count; ++i)
        if (g_panels[i].id && strcmp(g_panels[i].id, id) == 0)
            return i;
    return -1;
}

int overlay_set_panel_visible(const char* id, int visible)
{
    int i = overlay_find_panel_index(id);
    if (i < 0)
        return -1;
    return overlay_set_panel_visible_by_index(i, visible);
}

int overlay_get_panel_visible(const char* id)
{
    int i = overlay_find_panel_index(id);
    if (i < 0)
        return 0;
    return overlay_get_panel_visible_by_index(i);
}

/* Movable + persisted panel begin helper */
int overlay_begin_panel_auto(const char* id, const char* title, int default_x, int default_y,
                             int default_w)
{
    int x = default_x, y = default_y, w = default_w;
    int vis = 1;
    (void) overlay_layout_get(id, &x, &y, &w, &vis);
    if (!overlay_get_panel_visible(id))
        return 0;
    /* Drag handling on title bar */
    const OverlayInputState* in = overlay_input_get();
    int drag_h = 24;
    int panel_h = g_app.viewport_h - 20; /* clamp height to screen for overflow prevention */
    if (panel_h < 120)
        panel_h = 120;
    /* Title bar hit test & drag */
    static int dragging = 0;
    static int drag_dx = 0, drag_dy = 0;
    if (in)
    {
        if (!dragging && in->mouse_clicked && in->mouse_y >= y && in->mouse_y < y + drag_h &&
            in->mouse_x >= x && in->mouse_x < x + w)
        {
            dragging = 1;
            drag_dx = in->mouse_x - x;
            drag_dy = in->mouse_y - y;
        }
        else if (dragging && in->mouse_down)
        {
            x = in->mouse_x - drag_dx;
            y = in->mouse_y - drag_dy;
            if (x < 10)
                x = 10;
            if (y < 10)
                y = 10;
            if (x + w > g_app.viewport_w - 10)
                x = g_app.viewport_w - 10 - w;
            if (y + panel_h > g_app.viewport_h - 10)
                y = g_app.viewport_h - 10 - panel_h;
        }
        else if (dragging && !in->mouse_down)
        {
            dragging = 0;
            overlay_layout_set(id, x, y, w, vis);
            overlay_layout_save();
        }
    }
    /* Draw panel at computed position; override default height with clamped one */
#ifdef ROGUE_HAVE_SDL
    if (g_app.renderer)
    {
        const OverlayTheme* th = overlay_theme_get();
        SDL_Rect panel = {x, y, w, panel_h};
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_bg.r, th->panel_bg.g, th->panel_bg.b,
                               th->panel_bg.a);
        SDL_RenderFillRect(g_app.renderer, &panel);
        SDL_SetRenderDrawColor(g_app.renderer, th->panel_border.r, th->panel_border.g,
                               th->panel_border.b, th->panel_border.a);
        SDL_RenderDrawRect(g_app.renderer, &panel);
        if (title)
            rogue_font_draw_text(x + 6, y + 6, title, 1,
                                 (RogueColor){th->title_text.r, th->title_text.g, th->title_text.b,
                                              th->title_text.a});
    }
#endif
    /* Initialize UI cursor inside the panel, similar to overlay_begin_panel */
    g_ui.panel_active = 1;
    g_ui.cur_x = x + 8;
    g_ui.cur_y = y + 28;
    g_ui.width = w - 16;
    g_ui.line_h = 22;
    g_ui.columns = 1;
    g_ui.col_index = 0;
    g_ui.col_widths[0] = g_ui.width;
    g_ui.col_x0[0] = g_ui.cur_x;
    g_ui.row_start_y = g_ui.cur_y;
    g_ui.row_max_h = g_ui.line_h;
    g_ui.total_widgets = 0;
    g_ui.table_active = 0;
    g_ui.table_cols = 0;
    g_ui.table_row_h = 18;
    return 1;
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
