#include "start_screen.h"
#include "core/game_loop.h"
#include "core/save_manager.h" /* for simple Continue/Load stubs (slot 0) */
#include "graphics/font.h"
#include "graphics/sprite.h"
#include "input/input.h"
#include "util/log.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rogue_start_screen_active(void) { return g_app.show_start_screen; }

void rogue_start_screen_set_bg_scale(RogueStartBGScale mode) { g_app.start_bg_scale = (int) mode; }

/* Portable file existence check used to detect presence of a save file for Continue/Load. */
static int rogue_file_exists(const char* path)
{
#if defined(_MSC_VER)
    FILE* f = NULL;
    if (fopen_s(&f, path, "rb") == 0 && f)
    {
        fclose(f);
        return 1;
    }
    return 0;
#else
    FILE* f = fopen(path, "rb");
    if (f)
    {
        fclose(f);
        return 1;
    }
    return 0;
#endif
}

static void ensure_start_bg_loaded(void)
{
#ifdef ROGUE_HAVE_SDL
    static int attempted = 0; /* avoid retrying (and spamming warnings) every frame */
    if (g_app.start_bg_loaded || attempted)
        return;
    static RogueTexture tex; /* lifetime static; pointer stored in g_app */
    /* Resolve path with simple search order: env override -> assets/ -> ../assets/ */
    /* Use secure getenv variant on MSVC */
    char env_buf[512] = {0};
    {
#if defined(_MSC_VER)
        char* val = NULL;
        size_t len = 0;
        if (_dupenv_s(&val, &len, "ROGUE_START_BG") == 0 && val && val[0])
        {
            strncpy_s(env_buf, sizeof env_buf, val, _TRUNCATE);
        }
        if (val)
            free(val);
#else
        const char* v = getenv("ROGUE_START_BG");
        if (v && v[0])
        {
            strncpy(env_buf, v, sizeof env_buf - 1);
            env_buf[sizeof env_buf - 1] = '\0';
        }
#endif
    }
    const char* candidates[] = {
        env_buf[0] ? env_buf : NULL,
        "assets/vfx/start_bg.jpg",
        "../assets/vfx/start_bg.jpg",
    };
    const char* loaded_from = NULL;
    for (int i = 0; i < (int) (sizeof(candidates) / sizeof(candidates[0])); ++i)
    {
        const char* path = candidates[i];
        if (!path)
            continue;
        if (rogue_texture_load(&tex, path))
        {
            loaded_from = path;
            break;
        }
    }
    if (loaded_from)
    {
        g_app.start_bg_tex = &tex;
        g_app.start_bg_loaded = 1;
        g_app.start_bg_tint = 0xFFFFFFFFu;
        ROGUE_LOG_INFO("Start background loaded: %s", loaded_from);
    }
    else
    {
        g_app.start_bg_tex = NULL;
        g_app.start_bg_loaded = 0;
        ROGUE_LOG_WARN("Start background image not found; using gradient fallback");
    }
    attempted = 1;
#endif
}

static void render_background(void)
{
#ifdef ROGUE_HAVE_SDL
    ensure_start_bg_loaded();
    if (g_app.start_bg_loaded && g_app.start_bg_tex && g_app.start_bg_tex->handle)
    {
        /* Compute cover/contain scaling */
        int vw = g_app.viewport_w, vh = g_app.viewport_h;
        int iw = g_app.start_bg_tex->w, ih = g_app.start_bg_tex->h;
        float sx = (float) vw / (float) iw;
        float sy = (float) vh / (float) ih;
        float s = sx;
        if (g_app.start_bg_scale == ROGUE_BG_COVER)
            s = sx > sy ? sx : sy;
        else if (g_app.start_bg_scale == ROGUE_BG_CONTAIN)
            s = sx < sy ? sx : sy;
        int dw = (int) (iw * s);
        int dh = (int) (ih * s);
        int dx = (vw - dw) / 2;
        int dy = (vh - dh) / 2;
        SDL_Rect src = {0, 0, iw, ih};
        SDL_Rect dst = {dx, dy, dw, dh};
        extern SDL_Renderer* g_internal_sdl_renderer_ref;
        SDL_SetTextureColorMod(g_app.start_bg_tex->handle, (g_app.start_bg_tint >> 16) & 255,
                               (g_app.start_bg_tint >> 8) & 255, g_app.start_bg_tint & 255);
        SDL_SetTextureAlphaMod(g_app.start_bg_tex->handle, (g_app.start_bg_tint >> 24) & 255);
        SDL_RenderCopy(g_internal_sdl_renderer_ref, g_app.start_bg_tex->handle, &src, &dst);
    }
    else
    {
        /* Gradient fallback (vertical) */
        extern SDL_Renderer* g_internal_sdl_renderer_ref;
        for (int y = 0; y < g_app.viewport_h; ++y)
        {
            float t = (float) y / (float) (g_app.viewport_h - 1);
            unsigned char r = (unsigned char) (10 + (int) (30 * t));
            unsigned char g = (unsigned char) (15 + (int) (40 * t));
            unsigned char b = (unsigned char) (30 + (int) (80 * t));
            SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref, r, g, b, 255);
            SDL_RenderDrawLine(g_internal_sdl_renderer_ref, 0, y, g_app.viewport_w, y);
        }
    }
#endif
}

void rogue_start_screen_update_and_render(void)
{
    if (!g_app.show_start_screen)
        return;
    g_app.title_time += g_app.dt;
    /* Phase 1.1/1.2: simple fade in/out state machine */
    if (g_app.start_state_speed <= 0.0f)
        g_app.start_state_speed = 1.0f; /* default 1x per second */
    if (g_app.start_state == ROGUE_START_FADE_IN)
    {
        g_app.start_state_t += (float) g_app.dt * g_app.start_state_speed;
        if (g_app.start_state_t >= 1.0f)
        {
            g_app.start_state_t = 1.0f;
            g_app.start_state = ROGUE_START_MENU;
        }
    }
    else if (g_app.start_state == ROGUE_START_FADE_OUT)
    {
        g_app.start_state_t -= (float) g_app.dt * g_app.start_state_speed;
        if (g_app.start_state_t <= 0.0f)
        {
            g_app.start_state_t = 0.0f;
            g_app.show_start_screen = 0; /* transition complete */
        }
    }

    /* Background */
    render_background();
    RogueColor white = {255, 255, 255, 255};
    int pulse = (int) ((sin(g_app.title_time * 2.0) * 0.5 + 0.5) * 255.0);
    /* Title with fade alpha */
    int a = (int) (255.0f * (g_app.start_state == ROGUE_START_FADE_IN
                                 ? g_app.start_state_t
                                 : (g_app.start_state == ROGUE_START_FADE_OUT ? g_app.start_state_t
                                                                              : 1.0f)));
    RogueColor title_col2 = {(unsigned char) pulse, (unsigned char) pulse, 255, (unsigned char) a};
    rogue_font_draw_text(40, 60, "ROGUELIKE", 6, title_col2);
    /* Phase 3.1: Expanded main menu (Continue, New, Load, Settings, Credits, Quit) */
    /* Detect if any save exists (simple heuristic: presence of slot 0 in root or build/) */
    int has_save = 0;
    {
        const char* paths[] = {"save_slot_0.sav", "build/save_slot_0.sav"};
        for (int i = 0; i < (int) (sizeof(paths) / sizeof(paths[0])); ++i)
        {
            if (rogue_file_exists(paths[i]))
            {
                has_save = 1;
                break;
            }
        }
    }
    const char* menu_items[] = {"Continue", "New Game", "Load Game", "Settings",
                                "Credits",  "Quit",     "Seed:"};
    int enabled[] = {has_save, 1, has_save, 1, 1, 1, 1};
    int item_count = 7;
    int base_y = 140;
    /* Ensure current selection lands on an enabled item */
    if (!enabled[g_app.menu_index])
    {
        int tries = 0;
        while (!enabled[g_app.menu_index] && tries < item_count)
        {
            g_app.menu_index = (g_app.menu_index + 1) % item_count;
            tries++;
        }
    }
    for (int i = 0; i < item_count; i++)
    {
        RogueColor c;
        if (!enabled[i])
            c = (RogueColor){120, 120, 120, 255}; /* disabled */
        else if (i == g_app.menu_index)
            c = (RogueColor){255, 255, 0, 255}; /* selected */
        else
            c = white;
        rogue_font_draw_text(50, base_y + i * 20, menu_items[i], 2, c);
    }
    /* Seed entry shown on last line */
    char seed_line[64];
    snprintf(seed_line, sizeof seed_line, "%u", g_app.pending_seed);
    rogue_font_draw_text(140, base_y + (item_count - 1) * 20, seed_line, 2, white);
    if (g_app.entering_seed)
        rogue_font_draw_text(140 + (int) strlen(seed_line) * 12, base_y + (item_count - 1) * 20,
                             "_", 2, white);
    /* Navigation with wrap-around, skipping disabled items; includes key/axis repeat */
    int step_v = 0;
    int up_pressed = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_UP);
    int down_pressed = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DOWN);
    int up_down = rogue_input_is_down(&g_app.input, ROGUE_KEY_UP) ? 1 : 0;
    int down_down = rogue_input_is_down(&g_app.input, ROGUE_KEY_DOWN) ? 1 : 0;

    /* Immediate transitions on initial press */
    if (down_pressed)
        step_v = 1;
    else if (up_pressed)
        step_v = -1;

    /* Repeat handling when held */
    int held_dir = (down_down ? 1 : 0) + (up_down ? -1 : 0);
    if (held_dir == 0)
    {
        g_app.start_nav_repeating = 0;
        g_app.start_nav_accum_ms = 0.0;
        g_app.start_nav_dir_v = 0;
    }
    else
    {
        if (!g_app.start_nav_repeating || g_app.start_nav_dir_v != held_dir)
        {
            /* start fresh for new hold or direction change */
            g_app.start_nav_repeating = 1;
            g_app.start_nav_dir_v = held_dir;
            g_app.start_nav_accum_ms = 0.0;
        }
        else
        {
            g_app.start_nav_accum_ms += (g_app.dt * 1000.0);
            if (g_app.start_nav_accum_ms >= g_app.start_nav_initial_ms)
            {
                /* Emit repeat pulses at interval; may generate multiple on big dt */
                double over = g_app.start_nav_accum_ms - g_app.start_nav_initial_ms;
                int pulses = 1 + (int) (over / g_app.start_nav_interval_ms);
                /* Retain fractional remainder to keep cadence smooth */
                g_app.start_nav_accum_ms = g_app.start_nav_initial_ms +
                                           (over - (pulses - 1) * g_app.start_nav_interval_ms);
                step_v += pulses * g_app.start_nav_dir_v;
            }
        }
    }

    /* Apply vertical steps, one item at a time, honoring disabled entries */
    while (step_v > 0)
    {
        int next = (g_app.menu_index + 1) % item_count;
        for (int k = 0; k < item_count; ++k)
        {
            if (enabled[next])
            {
                g_app.menu_index = next;
                break;
            }
            next = (next + 1) % item_count;
        }
        step_v--;
    }
    while (step_v < 0)
    {
        int prev = (g_app.menu_index + item_count - 1) % item_count;
        for (int k = 0; k < item_count; ++k)
        {
            if (enabled[prev])
            {
                g_app.menu_index = prev;
                break;
            }
            prev = (prev + item_count - 1) % item_count;
        }
        step_v++;
    }
    /* Letter accelerators (only when not entering seed): jump to first item starting with char */
    if (!g_app.entering_seed && g_app.input.text_len > 0)
    {
        char ch = g_app.input.text_buffer[0];
        if (ch >= 'a' && ch <= 'z')
            ch = (char) (ch - 'a' + 'A');
        int start = (g_app.menu_index + 1) % item_count;
        for (int k = 0; k < item_count; ++k)
        {
            int idx = (start + k) % item_count;
            if (enabled[idx] && menu_items[idx][0] == ch)
            {
                g_app.menu_index = idx;
                break;
            }
        }
    }
    /* Accept with SPACE (ACTION) or ENTER (DIALOGUE) */
    if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION) ||
        rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DIALOGUE))
    {
        int sel = g_app.menu_index;
        if (!enabled[sel])
        {
            /* ignore activation on disabled items */
        }
        else if (sel == 0)
        { /* Continue */
            /* Try to load slot 0; on success transition into game */
            int rc = rogue_save_manager_load_slot(0);
            (void) rc; /* ignore errors for now; stay on menu if fails */
            if (rc == 0)
                g_app.start_state = ROGUE_START_FADE_OUT;
        }
        else if (sel == 1)
        { /* New Game */
            g_app.start_state = ROGUE_START_FADE_OUT;
        }
        else if (sel == 2)
        { /* Load Game (slot 0 stub) */
            int rc = rogue_save_manager_load_slot(0);
            (void) rc;
            if (rc == 0)
                g_app.start_state = ROGUE_START_FADE_OUT;
        }
        else if (sel == 3)
        { /* Settings (placeholder text only) */
            /* No-op: tooltip shown below title to indicate placeholder */
        }
        else if (sel == 4)
        { /* Credits (placeholder text only) */
            /* No-op */
        }
        else if (sel == 5)
        { /* Quit */
            rogue_game_loop_request_exit();
        }
        else if (sel == 6)
        { /* Seed */
            g_app.entering_seed = 1;
        }
    }
    /* Cancel/back: if entering seed, exit seed mode; else show a small hint */
    if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_CANCEL))
    {
        if (g_app.entering_seed)
            g_app.entering_seed = 0;
        else
            rogue_font_draw_text(50, base_y + item_count * 20 + 10,
                                 "Press Enter to select, Esc to go back", 2, white);
    }
    /* Simple help text for placeholder entries */
    if (g_app.menu_index == 3)
        rogue_font_draw_text(50, base_y + item_count * 20 + 10, "Settings coming soon", 2, white);
    if (g_app.menu_index == 4)
        rogue_font_draw_text(50, base_y + item_count * 20 + 10, "Credits coming soon", 2, white);
    if (g_app.entering_seed)
    {
        for (int i = 0; i < g_app.input.text_len; i++)
        {
            char ch = g_app.input.text_buffer[i];
            if (ch >= '0' && ch <= '9')
                g_app.pending_seed = g_app.pending_seed * 10 + (unsigned) (ch - '0');
            else if (ch == 'b' || ch == 'B')
                g_app.pending_seed /= 10;
        }
    }
}
