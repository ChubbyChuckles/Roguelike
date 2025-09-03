#include "overlay_core.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../content/json_io.h"
#include "overlay_commands.h"
#include "overlay_input.h"
#include "overlay_prefs.h"
#include "overlay_search.h"
#include "overlay_theme.h"
#include "overlay_toast.h"
#include "overlay_tooltip.h"
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

/* Navigation history (lightweight ring) */
#define OVERLAY_NAV_HISTORY_MAX 64
static OverlayNavState g_nav_hist[OVERLAY_NAV_HISTORY_MAX];
static int g_nav_hist_count = 0;      /* valid entries */
static int g_nav_hist_cursor = -1;    /* index into g_nav_hist of current entry */
static int g_nav_suppress_record = 0; /* set to 1 when applying history to avoid re-record */

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
    overlay_prefs_init();
    /* Seed default commands for the Command Palette */
    overlay_register_default_commands();
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
        /* Esc clears widget focus in the active overlay */
        if (in && in->key_escape_pressed)
        {
            extern UiCtx g_ui; /* from overlay_widgets_internal.h */
            g_ui.focus_index = -1;
        }
        if (in && in->key_p_pressed && in->key_ctrl_down && in->key_shift_down)
        {
            /* toggle command palette */
            overlay_commands_toggle(1);
        }
        if (in && in->key_k_pressed && in->key_ctrl_down)
        {
            overlay_search_toggle(1);
        }
        /* '?' opens the Shortcuts panel for cheat sheet */
        if (in && in->key_question_pressed)
        {
            (void) overlay_set_panel_visible("shortcuts", 1);
        }
        /* History navigation: Alt+Left = Back, Alt+Right = Forward */
        if (in && in->key_alt_down && in->key_left_pressed)
        {
            (void) overlay_nav_back();
        }
        if (in && in->key_alt_down && in->key_right_pressed)
        {
            (void) overlay_nav_forward();
        }
        /* Alt+1..9 quick panel visibility toggles for common panels */
        if (in && in->key_alt_down)
        {
            if (in->key_1_pressed)
                (void) overlay_set_panel_visible("system", 1);
            if (in->key_2_pressed)
                (void) overlay_set_panel_visible("items", 1);
            if (in->key_3_pressed)
                (void) overlay_set_panel_visible("skills", 1);
            if (in->key_4_pressed)
                (void) overlay_set_panel_visible("map", 1);
            if (in->key_5_pressed)
                (void) overlay_set_panel_visible("audiovfx", 1);
            if (in->key_6_pressed)
                (void) overlay_set_panel_visible("entities", 1);
            if (in->key_7_pressed)
                (void) overlay_set_panel_visible("content_graph", 1);
            if (in->key_8_pressed)
                (void) overlay_set_panel_visible("validation", 1);
            if (in->key_9_pressed)
                (void) overlay_set_panel_visible("dialogue", 1);
        }
        /* Ctrl+S: contextual save in current panels that support it */
        if (in && in->key_ctrl_down && in->key_s_pressed)
        {
            /* Skills: save overrides if panel code exposes helper */
            extern void panel_skills_save_overrides_and_refresh(void);
            if (overlay_get_panel_visible("skills"))
            {
                panel_skills_save_overrides_and_refresh();
                overlay_toast_push(OVERLAY_TOAST_INFO, "Skills overrides saved (Ctrl+S)", 1500);
            }
            /* Map: save to currently set path if available via a weak symbol */
            extern int rogue_map_debug_save_json(const char* path);
            if (overlay_get_panel_visible("map"))
            {
                /* Default to build/map.json matching the panel default */
                int rc = rogue_map_debug_save_json("build/map.json");
                (void) rc;
                overlay_toast_push(OVERLAY_TOAST_INFO, "Map saved (Ctrl+S)", 1500);
            }
        }
        /* Ctrl+Z / Ctrl+Y pass-through for Map (other panels can hook later) */
        if (in && in->key_ctrl_down && in->key_z_pressed && overlay_get_panel_visible("map"))
        {
            extern int rogue_map_debug_undo(void);
            (void) rogue_map_debug_undo();
        }
        if (in && in->key_ctrl_down && in->key_y_pressed && overlay_get_panel_visible("map"))
        {
            extern int rogue_map_debug_redo(void);
            (void) rogue_map_debug_redo();
        }
    }
    /* Invoke panel callbacks; panels draw using SDL primitives and fonts */
    for (int i = 0; i < g_panel_count; ++i)
    {
        if (g_panels[i].fn && (g_panel_visible_mask & (1u << i)))
            g_panels[i].fn(g_panels[i].user);
    }
    /* Tooltip on top of panels */
    overlay_tooltip_render();
    /* Non-modal systems */
    overlay_toast_render();
    overlay_commands_render();
    overlay_search_render();
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

/* Navigation helpers ------------------------------------------------------- */
/* Weak hooks exported by panels (optional). Declared here to avoid headers coupling. */
void rogue_overlay_items_set_selected_index(int index);
void rogue_overlay_skills_set_selected_index(int index);

void overlay_nav_open_items_and_select(int item_index)
{
    (void) overlay_set_panel_visible("items", 1);
    /* If panel provides a setter, use it. Otherwise no-op. */
    rogue_overlay_items_set_selected_index(item_index);
    /* Record into history */
    OverlayNavState st;
    st.panel_id = "items";
    st.sel_index = item_index;
    st.crumb_a[0] = '\0';
    st.crumb_b[0] = '\0';
    st.crumb_c[0] = '\0';
    overlay_nav_push(&st);
}

void overlay_nav_open_skills_and_select(int skill_index)
{
    (void) overlay_set_panel_visible("skills", 1);
    rogue_overlay_skills_set_selected_index(skill_index);
    OverlayNavState st;
    st.panel_id = "skills";
    st.sel_index = skill_index;
    st.crumb_a[0] = '\0';
    st.crumb_b[0] = '\0';
    st.crumb_c[0] = '\0';
    overlay_nav_push(&st);
}

/* Internal: load a nav state (show panel + set selection) */
static void overlay_nav_apply(const OverlayNavState* st)
{
    if (!st || !st->panel_id)
        return;
    g_nav_suppress_record = 1;
    if (strcmp(st->panel_id, "items") == 0)
    {
        overlay_set_panel_visible("items", 1);
        rogue_overlay_items_set_selected_index(st->sel_index);
    }
    else if (strcmp(st->panel_id, "skills") == 0)
    {
        overlay_set_panel_visible("skills", 1);
        rogue_overlay_skills_set_selected_index(st->sel_index);
    }
    g_nav_suppress_record = 0;
}

/* Public API: record current state (panels call on selection changes) */
void overlay_nav_set_current(const OverlayNavState* st)
{
    if (!st || !st->panel_id)
        return;
    if (g_nav_suppress_record)
        return;
    /* Append or coalesce if same panel and selection */
    if (g_nav_hist_count > 0 && g_nav_hist_cursor >= 0)
    {
        OverlayNavState* cur = &g_nav_hist[g_nav_hist_cursor];
        if (cur->panel_id && strcmp(cur->panel_id, st->panel_id) == 0 &&
            cur->sel_index == st->sel_index)
        {
            /* Update crumbs only */
            strncpy(cur->crumb_a, st->crumb_a, sizeof cur->crumb_a - 1);
            strncpy(cur->crumb_b, st->crumb_b, sizeof cur->crumb_b - 1);
            strncpy(cur->crumb_c, st->crumb_c, sizeof cur->crumb_c - 1);
            cur->crumb_a[sizeof cur->crumb_a - 1] = '\0';
            cur->crumb_b[sizeof cur->crumb_b - 1] = '\0';
            cur->crumb_c[sizeof cur->crumb_c - 1] = '\0';
            return;
        }
    }
    overlay_nav_push(st);
}

void overlay_nav_push(const OverlayNavState* st)
{
    if (!st || !st->panel_id)
        return;
    /* If we're not at the tip, drop forward history */
    if (g_nav_hist_cursor + 1 < g_nav_hist_count)
    {
        g_nav_hist_count = g_nav_hist_cursor + 1;
    }
    /* Append new state (bounded) */
    if (g_nav_hist_count < OVERLAY_NAV_HISTORY_MAX)
    {
        g_nav_hist[g_nav_hist_count] = *st;
        g_nav_hist_cursor = g_nav_hist_count;
        g_nav_hist_count++;
    }
    else
    {
        /* Shift left to free the last slot */
        for (int i = 1; i < OVERLAY_NAV_HISTORY_MAX; ++i)
            g_nav_hist[i - 1] = g_nav_hist[i];
        g_nav_hist[OVERLAY_NAV_HISTORY_MAX - 1] = *st;
        g_nav_hist_cursor = OVERLAY_NAV_HISTORY_MAX - 1;
        g_nav_hist_count = OVERLAY_NAV_HISTORY_MAX;
    }
}

int overlay_nav_back(void)
{
    if (g_nav_hist_cursor > 0)
    {
        g_nav_hist_cursor--;
        overlay_nav_apply(&g_nav_hist[g_nav_hist_cursor]);
        return 1;
    }
    return 0;
}
int overlay_nav_forward(void)
{
    if (g_nav_hist_cursor + 1 < g_nav_hist_count)
    {
        g_nav_hist_cursor++;
        overlay_nav_apply(&g_nav_hist[g_nav_hist_cursor]);
        return 1;
    }
    return 0;
}

void overlay_nav_render_breadcrumb(const OverlayNavState* st)
{
    if (!st)
        return;
    char line[256];
    line[0] = '\0';
    const char* pfx = "";
    if (st->crumb_a[0])
    {
        strncat(line, pfx, sizeof line - strlen(line) - 1);
        strncat(line, st->crumb_a, sizeof line - strlen(line) - 1);
        pfx = " > ";
    }
    if (st->crumb_b[0])
    {
        strncat(line, pfx, sizeof line - strlen(line) - 1);
        strncat(line, st->crumb_b, sizeof line - strlen(line) - 1);
        pfx = " > ";
    }
    if (st->crumb_c[0])
    {
        strncat(line, pfx, sizeof line - strlen(line) - 1);
        strncat(line, st->crumb_c, sizeof line - strlen(line) - 1);
    }
    if (line[0])
        overlay_label(line);
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
