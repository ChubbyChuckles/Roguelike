#include "overlay_commands.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../core/app/app_state.h"
#include "../graphics/font.h"
#include "overlay_core.h"
#include "overlay_input.h"
#include "overlay_theme.h"
#include "overlay_toast.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#include <string.h>

typedef struct Cmd
{
    const char* name;
    OverlayCommandFn fn;
    void* user;
} Cmd;

#define CMD_MAX 64
static Cmd g_cmds[CMD_MAX];
static int g_cmd_count = 0;

static int g_open = 0;
static char g_filter[64];
static int g_selected = 0;

int overlay_command_register(const char* name, OverlayCommandFn fn, void* user)
{
    if (g_cmd_count >= CMD_MAX)
        return -1;
    g_cmds[g_cmd_count].name = name;
    g_cmds[g_cmd_count].fn = fn;
    g_cmds[g_cmd_count].user = user;
    return g_cmd_count++;
}

int overlay_invoke_action(const char* name)
{
    for (int i = 0; i < g_cmd_count; ++i)
        if (strcmp(g_cmds[i].name, name) == 0)
        {
            if (g_cmds[i].fn)
                g_cmds[i].fn(g_cmds[i].user);
            return 0;
        }
    return -1;
}

void overlay_commands_toggle(int open)
{
    g_open = open ? 1 : 0;
    if (g_open)
    {
        g_filter[0] = '\0';
        g_selected = 0;
    }
}

static int cmd_match(const char* name, const char* filter)
{
    if (!filter || !*filter)
        return 1;
    return strstr(name, filter) != NULL;
}

void overlay_commands_render(void)
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
    int y = (int) (g_app.viewport_h * 0.15f);
    int h = 28 + 6 + 8 * 20 + 8; /* input + spacing + 8 rows + pad */
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
        x + 8, y + 6, ":>", 1,
        (RogueColor){th->text_accent.r, th->text_accent.g, th->text_accent.b, th->text_accent.a});
    /* append incoming text this frame */
    const OverlayInputState* in = overlay_input_get();
    if (in->text_input[0])
    {
        size_t cur = strlen(g_filter);
        size_t avail = sizeof g_filter - 1 - cur;
        strncat(g_filter, in->text_input, avail);
    }
    if (in->key_backspace_pressed)
    {
        size_t l = strlen(g_filter);
        if (l > 0)
            g_filter[l - 1] = '\0';
    }
    rogue_font_draw_text(x + 30, y + 6, g_filter, 1,
                         (RogueColor){th->text.r, th->text.g, th->text.b, th->text.a});
    /* list */
    int shown = 0;
    int first_match_index = -1;
    int draw_y = y + 28 + 6;
    for (int i = 0; i < g_cmd_count && shown < 8; ++i)
    {
        if (!cmd_match(g_cmds[i].name, g_filter))
            continue;
        if (first_match_index < 0)
            first_match_index = i;
        int sel = (i == g_selected);
#ifdef ROGUE_HAVE_SDL
        SDL_Rect row = {x + 6, draw_y - 2, w - 12, 20};
        SDL_SetRenderDrawColor(g_app.renderer, sel ? th->table_row_bg_sel.r : th->table_row_bg.r,
                               sel ? th->table_row_bg_sel.g : th->table_row_bg.g,
                               sel ? th->table_row_bg_sel.b : th->table_row_bg.b,
                               sel ? th->table_row_bg_sel.a : th->table_row_bg.a);
        SDL_RenderFillRect(g_app.renderer, &row);
        SDL_SetRenderDrawColor(g_app.renderer, th->table_border.r, th->table_border.g,
                               th->table_border.b, th->table_border.a);
        SDL_RenderDrawRect(g_app.renderer, &row);
#endif
        rogue_font_draw_text(
            x + 12, draw_y, g_cmds[i].name, 1,
            (RogueColor){th->table_text.r, th->table_text.g, th->table_text.b, th->table_text.a});
        draw_y += 22;
        shown++;
    }
    /* navigation */
    if (in->key_down_pressed)
        g_selected++;
    if (in->key_up_pressed)
        g_selected--;
    if (g_selected < 0)
        g_selected = 0;
    if (g_selected >= g_cmd_count)
        g_selected = g_cmd_count - 1;
    if (in->key_enter_pressed && g_selected >= 0 && g_selected < g_cmd_count)
    {
        overlay_invoke_action(g_cmds[g_selected].name);
        overlay_commands_toggle(0);
    }
    if (in->key_escape_pressed)
        overlay_commands_toggle(0);
}

/* --- Default commands registry ----------------------------------------- */
static void cmd_validation_run(void* user)
{
    (void) user;
    /* Headless-safe run-now; shows results in Validation panel */
    extern int rogue_validation_run_now(int force_all);
    (void) rogue_validation_run_now(1);
    overlay_toast_push(OVERLAY_TOAST_INFO, "Validation started…", 1500);
}
static void cmd_validation_toggle_panel(void* user)
{
    (void) user;
    (void) overlay_set_panel_visible("validation", 1);
}
static void cmd_skills_save_overrides(void* user)
{
    (void) user;
    extern int rogue_skill_debug_save_overrides(const char* path);
    int rc = rogue_skill_debug_save_overrides("build/skills_overrides.json");
    overlay_toast_push((rc == 0) ? OVERLAY_TOAST_INFO : OVERLAY_TOAST_ERROR,
                       (rc == 0) ? "Skills overrides saved" : "Save overrides failed", 2000);
}
static void cmd_skills_load_overrides(void* user)
{
    (void) user;
    extern int rogue_skill_debug_load_overrides_file(const char* path);
    int rc = rogue_skill_debug_load_overrides_file("build/skills_overrides.json");
    overlay_toast_push((rc >= 0) ? OVERLAY_TOAST_INFO : OVERLAY_TOAST_ERROR,
                       (rc >= 0) ? "Skills overrides loaded" : "Load overrides failed", 2000);
}
static void cmd_content_graph_export_dot(void* user)
{
    (void) user;
    /* Reuse the panel helper by invoking the button-less export path directly here. */
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "build/content_graph.dot", "wb");
#else
    f = fopen("build/content_graph.dot", "wb");
#endif
    if (!f)
        return;
    fputs("digraph ContentGraph {\n  rankdir=LR;\n  node [shape=box,fontname=Helvetica];\n", f);
    extern int rogue_asset_dep_count(void);
    extern int rogue_asset_dep_get(int index, const char** out_id, const char** out_path);
    extern int rogue_asset_dep_get_deps(const char* id, const char** out_dep_ids, int max_out);
    int nall = rogue_asset_dep_count();
    for (int i = 0; i < nall; ++i)
    {
        const char *nid = NULL, *pp = NULL;
        if (rogue_asset_dep_get(i, &nid, &pp) != 0 || !nid)
            continue;
        char lbl[512];
        snprintf(lbl, sizeof lbl, "%s\\n%s", nid, (pp && *pp) ? pp : "<none>");
        for (char* p = lbl; *p; ++p)
            if (*p == '"')
                *p = '\'';
        fprintf(f, "  \"%s\" [label=\"%s\"];\n", nid, lbl);
        const char* deps[16];
        int dc = rogue_asset_dep_get_deps(nid, deps, (int) (sizeof deps / sizeof deps[0]));
        for (int j = 0; j < dc; ++j)
        {
            if (deps[j])
                fprintf(f, "  \"%s\" -> \"%s\";\n", nid, deps[j]);
        }
    }
    fputs("}\n", f);
    fclose(f);
}
static void cmd_open_panel_items(void* user)
{
    (void) user;
    (void) overlay_set_panel_visible("items", 1);
}
static void cmd_open_panel_skills(void* user)
{
    (void) user;
    (void) overlay_set_panel_visible("skills", 1);
}
static void cmd_open_panel_map(void* user)
{
    (void) user;
    (void) overlay_set_panel_visible("map", 1);
}

void overlay_register_default_commands(void)
{
    /* Idempotent: cheap linear scan to avoid duplicates */
    int need_register = 1;
    for (int i = 0; i < g_cmd_count; ++i)
        if (strcmp(g_cmds[i].name, "Validation: Run Now") == 0)
            need_register = 0;
    if (!need_register)
        return;
    overlay_command_register("Validation: Run Now", cmd_validation_run, NULL);
    overlay_command_register("Validation: Show Panel", cmd_validation_toggle_panel, NULL);
    overlay_command_register("Skills: Save Overrides JSON", cmd_skills_save_overrides, NULL);
    overlay_command_register("Skills: Load Overrides JSON", cmd_skills_load_overrides, NULL);
    overlay_command_register("Content Graph: Export DOT", cmd_content_graph_export_dot, NULL);
    overlay_command_register("Panels: Open Items", cmd_open_panel_items, NULL);
    overlay_command_register("Panels: Open Skills", cmd_open_panel_skills, NULL);
    overlay_command_register("Panels: Open Map", cmd_open_panel_map, NULL);
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
