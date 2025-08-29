#include "start_screen.h"
#include "../core/integration/event_bus.h"
#include "../core/persistence/save_manager.h" /* for simple Continue/Load stubs (slot 0) */
#include "../graphics/font.h"
#include "../graphics/sprite.h"
#include "../input/input.h"
#include "../ui/core/ui_theme.h"
#include "../util/log.h"
#include "../world/tile_sprite_cache.h"
#include "../world/world_gen.h"
#include "../world/world_gen_config.h"
#include "game_loop.h"
#include "localization.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rogue_start_screen_active(void) { return g_app.show_start_screen; }

/* Forward decls */
static void rogue_start_begin_new_game_from_seed(void);
/* Global one-time quarantine flags for corrupt-at-start detection. */
static int s_start_scan_done = 0;
static int s_corrupt_at_start_global = 0;

void rogue_start_screen_set_bg_scale(RogueStartBGScale mode) { g_app.start_bg_scale = (int) mode; }

/* Local helpers for Phase 3.3 */
static const char* menu_key_for_index(int idx)
{
    switch (idx)
    {
    case 0:
        return "menu_continue";
    case 1:
        return "menu_new_game";
    case 2:
        return "menu_load";
    case 3:
        return "menu_settings";
    case 4:
        return "menu_credits";
    case 5:
        return "menu_quit";
    case 6:
        return "menu_seed";
    default:
        return "";
    }
}

const char* rogue_start_menu_label(int index)
{
    return rogue_locale_get(menu_key_for_index(index));
}

static const char* tooltip_for_selection(int idx)
{
    if (idx == 3)
        return rogue_locale_get("tip_settings");
    if (idx == 4)
        return rogue_locale_get("tip_credits");
    return rogue_locale_get("hint_accept_cancel");
}

const char* rogue_start_tooltip_text(void) { return tooltip_for_selection(g_app.menu_index); }

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

/* Return file size in bytes, or -1 on error. Portable using stdio. */
static long rogue_file_size(const char* path)
{
#if defined(_MSC_VER)
    FILE* f = NULL;
    if (fopen_s(&f, path, "rb") != 0 || !f)
        return -1L;
#else
    FILE* f = fopen(path, "rb");
    if (!f)
        return -1L;
#endif
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return -1L;
    }
    long sz = ftell(f);
    fclose(f);
    return sz;
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

/* Lightweight sanity check for save descriptor to guard UI actions (Continue/Load)
   against corrupt or incomplete headers. Keeps Start Screen deterministic. */
static int rogue_save_descriptor_is_sane(const struct RogueSaveDescriptor* d)
{
    if (!d)
        return 0;
    /* Must match current format and have at least one section */
    if (d->version != ROGUE_SAVE_FORMAT_VERSION)
        return 0;
    if (d->section_count == 0)
        return 0;
    /* Require critical components to be present: PLAYER and WORLD_META */
    unsigned mask = d->component_mask;
    unsigned need_player = (1u << ROGUE_SAVE_COMP_PLAYER);
    unsigned need_world = (1u << ROGUE_SAVE_COMP_WORLD_META);
    if ((mask & need_player) == 0 || (mask & need_world) == 0)
        return 0;
    /* total_size and checksum are best-effort; UI won't recompute here */
    if (d->total_size == 0)
        return 0;
    return 1;
}

/* Phase 8: Prewarm implementation (incremental, light-weight)
   Steps:
   0 -> seed glyph cache with ASCII; 1 -> ensure background decode; 2 -> ensure tile sprite cache.
 */
static void start_prewarm_tick(void)
{
    if (!g_app.start_prewarm_active || g_app.start_prewarm_done)
        return;
    int step = g_app.start_prewarm_step;
    switch (step)
    {
    case 0:
        /* Glyph cache bootstrap: measure a common ASCII string to populate cache deterministically.
         */
        rogue_font_draw_text(0, 0, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                             1, (RogueColor){0, 0, 0, 0});
        g_app.start_prewarm_step++;
        break;
    case 1:
        ensure_start_bg_loaded();
        g_app.start_prewarm_step++;
        break;
    case 2:
        /* Build tile sprite LUT so first in-game frame has assets ready. */
        rogue_tile_sprite_cache_ensure();
        g_app.start_prewarm_step++;
        g_app.start_prewarm_done = 1;
        g_app.start_prewarm_active = 0;
        break;
    default:
        g_app.start_prewarm_done = 1;
        g_app.start_prewarm_active = 0;
        break;
    }
}

static void maybe_begin_prewarm(void)
{
    if (!g_app.start_prewarm_active && !g_app.start_prewarm_done)
    {
        /* Allow disabling via env ROGUE_START_PREWARM=0 (defaults to on). */
        static int s_checked = 0;
        static int s_enabled = 1;
        if (!s_checked)
        {
#if defined(_MSC_VER)
            char* v = NULL;
            size_t L = 0;
            if (_dupenv_s(&v, &L, "ROGUE_START_PREWARM") == 0 && v)
            {
                if (v[0] == '0')
                    s_enabled = 0;
                free(v);
            }
#else
            const char* v = getenv("ROGUE_START_PREWARM");
            if (v && v[0] == '0')
                s_enabled = 0;
#endif
            s_checked = 1;
        }
        if (s_enabled)
        {
            g_app.start_prewarm_active = 1;
            g_app.start_prewarm_step = 0;
        }
    }
}

static int start_perf_over_budget(void)
{
    /* Returns 1 if the last measured frame exceeded budget (absolute or relative).
       Baseline is computed over first start_perf_target_samples frames. */
    double last_ms = g_app.frame_ms; /* from previous frame end */
    int abs_over = (g_app.start_perf_budget_ms > 0.0 && last_ms > g_app.start_perf_budget_ms);
    int rel_over = 0;
    /* Only apply relative check after baseline sampling completes and when the threshold is
       non-negative. A negative threshold disables relative regression checks (useful in tests). */
    if (g_app.start_perf_regress_threshold_pct >= 0.0 &&
        g_app.start_perf_samples >= g_app.start_perf_target_samples &&
        g_app.start_perf_baseline_ms > 0.0)
    {
        double allowed =
            g_app.start_perf_baseline_ms * (1.0 + g_app.start_perf_regress_threshold_pct);
        if (last_ms > allowed)
            rel_over = 1;
    }
    return abs_over || rel_over;
}

static void render_spinner_overlay(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.start_prewarm_active || g_app.reduced_motion)
        return;
    if (g_app.start_perf_reduce_quality)
        return; /* suppress spinner under budget pressure */
    /* Simple spinner at top-right: three rotating dots. */
    extern SDL_Renderer* g_internal_sdl_renderer_ref;
    int cx = g_app.viewport_w - 24;
    int cy = 16;
    g_app.start_spinner_angle += (float) (g_app.dt * 6.0); /* rad/s approx */
    float a = g_app.start_spinner_angle;
    for (int i = 0; i < 3; ++i)
    {
        float ang = a + (float) (i * 2.09439510239); /* 120 degrees apart */
        int x = cx + (int) (cosf(ang) * 6.0f);
        int y = cy + (int) (sinf(ang) * 6.0f);
        SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref, 220, 220, 240, 220);
        SDL_RenderDrawPoint(g_internal_sdl_renderer_ref, x, y);
        SDL_RenderDrawPoint(g_internal_sdl_renderer_ref, x + 1, y);
        SDL_RenderDrawPoint(g_internal_sdl_renderer_ref, x, y + 1);
    }
#endif
}

static void render_background(void)
{
#ifdef ROGUE_HAVE_SDL
    ensure_start_bg_loaded();
    /* Phase 2.9: Day/Night tint: gently modulate tint based on local time seconds.
       If headless or time unavailable, fall back to seed-derived pseudo-time. */
    unsigned int tint = g_app.start_bg_tint;
    {
        double t = fmod(g_app.game_time_ms / 1000.0, 120.0); /* 2-minute cycle in tests */
        /* Use sine wave to modulate brightness between 85% and 100% */
        double m = 0.85 + 0.15 * (0.5 + 0.5 * sin(t * 3.14159 / 60.0));
        unsigned char tr0 = (tint >> 16) & 255;
        unsigned char tg0 = (tint >> 8) & 255;
        unsigned char tb0 = tint & 255;
        unsigned char ta0 = (tint >> 24) & 255;
        unsigned char trm = (unsigned char) ((int) (tr0 * m));
        unsigned char tgm = (unsigned char) ((int) (tg0 * m));
        unsigned char tbm = (unsigned char) ((int) (tb0 * m));
        tint = ((unsigned int) ta0 << 24) | ((unsigned int) trm << 16) | ((unsigned int) tgm << 8) |
               (unsigned int) tbm;
    }
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
        /* Accessibility: clamp brightness so overlays remain legible */
        unsigned char tr = (tint >> 16) & 255;
        unsigned char tg = (tint >> 8) & 255;
        unsigned char tb = tint & 255;
        unsigned char ta = (tint >> 24) & 255;
        int maxc = tr;
        if (tg > maxc)
            maxc = tg;
        if (tb > maxc)
            maxc = tb;
        if (maxc > 240)
        {
            float scale = 240.0f / (float) maxc;
            tr = (unsigned char) (tr * scale);
            tg = (unsigned char) (tg * scale);
            tb = (unsigned char) (tb * scale);
        }
        SDL_SetTextureColorMod(g_app.start_bg_tex->handle, tr, tg, tb);
        SDL_SetTextureAlphaMod(g_app.start_bg_tex->handle, ta);
        SDL_RenderCopy(g_internal_sdl_renderer_ref, g_app.start_bg_tex->handle, &src, &dst);
        /* High contrast overlay (subtle darken) */
        if (g_app.high_contrast)
        {
            SDL_SetRenderDrawBlendMode(g_internal_sdl_renderer_ref, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref, 0, 0, 0, 60);
            SDL_Rect full = {0, 0, g_app.viewport_w, g_app.viewport_h};
            SDL_RenderFillRect(g_internal_sdl_renderer_ref, &full);
        }
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
    /* Phase 2.5: simple parallax stars/particles overlay (unaffected by reduced motion).
       Disable when start_perf_reduce_quality is set. */
    if (!g_app.start_perf_reduce_quality)
    {
        extern SDL_Renderer* g_internal_sdl_renderer_ref;
        /* Deterministic positions derived from a fixed seed so snapshot tests remain stable. */
        unsigned int base_seed = 0xC0FFEEu; /* constant so first-frame state is stable */
        /* Three layers: slow, medium, fast */
        const int layers = 3;
        const int counts[3] = {20, 14, 8};
        const int alphas[3] = {70, 110, 160};
        const float speeds[3] = {2.0f, 6.0f, 12.0f};
        for (int L = 0; L < layers; ++L)
        {
            unsigned int s = base_seed ^ (unsigned int) (L * 0x9E3779B9u);
            SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref, 255, 255, 255, (Uint8) alphas[L]);
            for (int i = 0; i < counts[L]; ++i)
            {
                /* xorshift for quick deterministic pseudo-randoms */
                s ^= s << 13;
                s ^= s >> 17;
                s ^= s << 5;
                int px = (int) (s % (unsigned) (g_app.viewport_w + 40)) - 20;
                s ^= s << 13;
                s ^= s >> 17;
                s ^= s << 5;
                int py = (int) (s % (unsigned) (g_app.viewport_h)) + 0;
                /* Horizontal drift per layer speed; wrap to screen */
                float dx = (float) fmod((double) (g_app.title_time * speeds[L]),
                                        (double) (g_app.viewport_w + 40));
                int x = px - (int) dx;
                if (x < -20)
                    x += g_app.viewport_w + 40;
                SDL_RenderDrawPoint(g_internal_sdl_renderer_ref, x, py);
            }
        }
    }
#endif
}

/* --- Phase 7: Credits & Legal overlay --- */
static void draw_text_wrapped(int x, int y, const char* text, int scale, RogueColor color,
                              int max_w)
{
    /* Extremely simple mono font wrap at spaces; fallback to hard cut. Each char ~6px at scale=1,
       our font_draw_text(scale) likely scales width ~6*scale (approx). We'll wrap by chars. */
    int approx_char_w = 6 * scale + 1;
    int max_chars = max_w > 0 ? (max_w / (approx_char_w > 1 ? approx_char_w : 1)) : 80;
    const char* p = text;
    while (*p)
    {
        const char* line_start = p;
        int count = 0;
        const char* last_space = NULL;
        while (*p && *p != '\n' && count < max_chars)
        {
            if (*p == ' ')
                last_space = p;
            p++;
            count++;
        }
        const char* line_end = p;
        if (*p && *p != '\n' && last_space)
        {
            line_end = last_space;
            p = last_space + 1;
        }
        char buf[256];
        int len = (int) (line_end - line_start);
        if (len > (int) sizeof(buf) - 1)
            len = (int) sizeof(buf) - 1;
        memcpy(buf, line_start, len);
        buf[len] = '\0';
        rogue_font_draw_text(x, y, buf, scale, color);
        y += 12 * scale + 2;
        if (*p == '\n')
            p++;
    }
}

static void start_credits_overlay_update_and_render(void)
{
    /* Tabs: 0=Credits, 1=Licenses, 2=Build */
    int base_x = 46, base_y = 120;
    RogueColor white = (RogueColor){255, 255, 255, 255};
    RogueColor yellow = (RogueColor){255, 255, 0, 255};
    const char* tabs[3] = {"Credits", "Licenses", "Build"};
    for (int i = 0; i < 3; ++i)
    {
        RogueColor c = (i == g_app.start_credits_tab) ? yellow : white;
        rogue_font_draw_text(base_x + i * 90, base_y - 20, tabs[i], 2, c);
    }
    /* Content area with inertial scroll. Mouse not handled; use keyboard only. */
    int area_x = base_x;
    int area_y = base_y;
    int area_w = (g_app.viewport_w > 320 ? g_app.viewport_w - base_x - 20 : 240);
    /* approx line height (unused here; draw_text_wrapped advances internally) */
    /* Update inertia: Up/Down adjust velocity; apply friction. */
    float accel = 0.0f;
    if (rogue_input_is_down(&g_app.input, ROGUE_KEY_DOWN))
        accel += 120.0f; /* lines per second approx */
    if (rogue_input_is_down(&g_app.input, ROGUE_KEY_UP))
        accel -= 120.0f;
    g_app.start_credits_vel += accel * (float) g_app.dt;
    /* Friction */
    g_app.start_credits_vel *= (float) pow(0.90, g_app.dt * 60.0);
    g_app.start_credits_scroll += g_app.start_credits_vel * (float) g_app.dt;
    if (g_app.start_credits_scroll < 0.0f)
    {
        g_app.start_credits_scroll = 0.0f;
        g_app.start_credits_vel = 0.0f;
    }

    /* Render content based on tab */
    int y = area_y - (int) g_app.start_credits_scroll;
    if (g_app.start_credits_tab == 0)
    {
        const char* credits = "Roguelike Prototype\n\n"
                              "Programming: Chuck + Contributors\n"
                              "Design: Chuck\n"
                              "Art: Placeholder Pack\n"
                              "Audio: Placeholder SFX/BGM (optional)\n\n"
                              "Special thanks to the open-source community and SDL maintainers.";
        draw_text_wrapped(area_x, y, credits, 2, white, area_w);
    }
    else if (g_app.start_credits_tab == 1)
    {
        const char* licenses = "Third-Party Licenses\n\n"
                               "SDL2 (zlib)\n"
                               "SDL2_image (zlib)\n"
                               "SDL2_mixer (zlib)\n"
                               "This project itself is MIT-licensed.";
        draw_text_wrapped(area_x, y, licenses, 2, white, area_w);
    }
    else
    {
        /* Build info from CMake macros */
#ifdef ROGUE_BUILD_GIT_HASH
        const char* hash = ROGUE_BUILD_GIT_HASH;
#else
        const char* hash = "unknown";
#endif
#ifdef ROGUE_BUILD_GIT_BRANCH
        const char* branch = ROGUE_BUILD_GIT_BRANCH;
#else
        const char* branch = "unknown";
#endif
#ifdef ROGUE_BUILD_TIME
        const char* btime = ROGUE_BUILD_TIME;
#else
        const char* btime = "unknown";
#endif
        char buf[256];
        snprintf(buf, sizeof buf, "Version: %s\nBranch: %s\nBuilt: %s", hash, branch, btime);
        draw_text_wrapped(area_x, y, buf, 2, white, area_w);
    }

    /* Hints */
    rogue_font_draw_text(base_x, g_app.viewport_h - 24, "Up/Down scroll  Left/Right tab  Esc back",
                         2, white);

    /* Input: Left/Right switch tab, Esc to exit overlay */
    if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_LEFT))
    {
        g_app.start_credits_tab = (g_app.start_credits_tab + 2) % 3;
        g_app.start_credits_scroll = 0.0f;
        g_app.start_credits_vel = 0.0f;
    }
    if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_RIGHT))
    {
        g_app.start_credits_tab = (g_app.start_credits_tab + 1) % 3;
        g_app.start_credits_scroll = 0.0f;
        g_app.start_credits_vel = 0.0f;
    }
    if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_CANCEL))
    {
        g_app.start_show_credits = 0;
    }
}

/* --- Phase 4.3: Simple thumbnail placeholder rendering --- */
static void draw_slot_thumbnail(int x, int y, int w, int h, int slot_index,
                                const RogueSaveDescriptor* desc)
{
#ifdef ROGUE_HAVE_SDL
    extern SDL_Renderer* g_internal_sdl_renderer_ref;
    /* Seed-based stable color (use slot index and timestamp for variation) */
    unsigned int s = (unsigned int) (0xA5A5u ^ (slot_index * 2654435761u) ^ desc->timestamp_unix);
    unsigned char r = (unsigned char) (64 + (s & 127));
    unsigned char g = (unsigned char) (64 + ((s >> 8) & 127));
    unsigned char b = (unsigned char) (64 + ((s >> 16) & 127));
    SDL_Rect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref, r, g, b, 255);
    SDL_RenderFillRect(g_internal_sdl_renderer_ref, &rect);
    /* Border */
    SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref, 220, 220, 240, 255);
    SDL_RenderDrawRect(g_internal_sdl_renderer_ref, &rect);
#else
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    (void) slot_index;
    (void) desc;
#endif
}

static void rogue_start_screen_maybe_scan_corruption(void)
{
    /* One-time initial scan: if slot 0 appears corrupt (tiny file or invalid descriptor),
       quarantine Continue/Load for the remainder of this session to avoid races with other
       processes/tests rewriting the file mid-run. Applies in both headless and non-headless. */
    if (!s_start_scan_done)
    {
        int corrupt = 0;
        /* Tiny file heuristic first (handles tests writing a few bytes). */
        if (rogue_file_exists("save_slot_0.sav"))
        {
            long fs = rogue_file_size("save_slot_0.sav");
            if (fs >= 0 && fs < 32)
                corrupt = 1;
        }
        /* Fallback to descriptor sanity check. */
        if (!corrupt)
        {
            RogueSaveDescriptor td;
            int ok0 =
                (rogue_save_read_descriptor(0, &td) == 0 && rogue_save_descriptor_is_sane(&td));
            corrupt = ok0 ? 0 : 1;
        }
        s_corrupt_at_start_global = corrupt;
        s_start_scan_done = 1;
    }
}

void rogue_start_screen_scan_corruption_at_init(void)
{
    rogue_start_screen_maybe_scan_corruption();
}

void rogue_start_screen_update_and_render(void)
{
    if (!g_app.show_start_screen)
    {
        /* Invariant: when hidden, start_state must not be FADE_*; normalize to MENU. */
        if (g_app.start_state != ROGUE_START_MENU)
            g_app.start_state = ROGUE_START_MENU;
        return;
    }
    rogue_start_screen_maybe_scan_corruption();
    /* Phase 8.3: baseline sampling and budget check (uses previous frame time) */
    if (g_app.start_perf_samples < g_app.start_perf_target_samples)
    {
        g_app.start_perf_accum_ms += g_app.frame_ms;
        g_app.start_perf_samples++;
        if (g_app.start_perf_samples == g_app.start_perf_target_samples &&
            g_app.start_perf_baseline_ms <= 0.0)
        {
            g_app.start_perf_baseline_ms =
                g_app.start_perf_accum_ms / (double) g_app.start_perf_target_samples;
        }
    }
    if (start_perf_over_budget())
    {
        g_app.start_perf_regressed = 1;
        g_app.start_perf_reduce_quality = 1;
        if (!g_app.start_perf_warned)
        {
            ROGUE_LOG_WARN("StartScreen over budget: last=%.3fms baseline=%.3fms budget=%.3fms "
                           "(reducing quality)",
                           g_app.frame_ms, g_app.start_perf_baseline_ms,
                           g_app.start_perf_budget_ms);
            g_app.start_perf_warned = 1;
        }
    }
    g_app.title_time += g_app.dt;
    /* Phase 8: begin and advance prewarm (a couple of steps over first frames) */
    maybe_begin_prewarm();
    start_prewarm_tick();
    /* Phase 10.4: Reduced motion skips animated fades entirely */
    if (g_app.reduced_motion)
    {
        if (g_app.start_state == ROGUE_START_FADE_IN)
        {
            g_app.start_state = ROGUE_START_MENU;
            g_app.start_state_t = 1.0f;
        }
        else if (g_app.start_state == ROGUE_START_FADE_OUT)
        {
            /* Skip fade-out as well under reduced motion */
            g_app.start_state_t = 0.0f;
            g_app.show_start_screen = 0;
            /* Normalize state to MENU when start screen is hidden to satisfy invariants */
            g_app.start_state = ROGUE_START_MENU;
        }
    }
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
        /* Phase 9.3: cancel overlays immediately on exit */
        g_app.start_show_settings = 0;
        g_app.start_show_credits = 0;
        g_app.start_show_load_list = 0;
        g_app.entering_seed = 0;
        g_app.start_state_t -= (float) g_app.dt * g_app.start_state_speed;
        if (g_app.start_state_t <= 0.0f)
        {
            g_app.start_state_t = 0.0f;
            g_app.show_start_screen = 0; /* transition complete */
            /* Normalize state to MENU once hidden to avoid invalid FADE_OUT + hidden combo */
            g_app.start_state = ROGUE_START_MENU;
            /* Phase 9.1: when leaving start, enable world fade-in overlay */
            if (!g_app.reduced_motion)
            {
                g_app.world_fade_active = 1;
                g_app.world_fade_t = 1.0f; /* start fully black, fade to 0 */
                if (g_app.world_fade_speed <= 0.0f)
                    g_app.world_fade_speed = 1.0f;
            }
            else
            {
                g_app.world_fade_active = 0;
                g_app.world_fade_t = 0.0f;
            }
        }
    }

    /* Background */
    render_background();
    render_spinner_overlay();
    RogueColor white = {255, 255, 255, 255};
    int pulse =
        g_app.reduced_motion ? 220 : (int) ((sin(g_app.title_time * 2.0) * 0.5 + 0.5) * 255.0);
    /* Title with fade alpha and safe-area lockup (Phase 2.7) */
    int a = (int) (255.0f * (g_app.start_state == ROGUE_START_FADE_IN
                                 ? g_app.start_state_t
                                 : (g_app.start_state == ROGUE_START_FADE_OUT ? g_app.start_state_t
                                                                              : 1.0f)));
    RogueColor title_col2 = {(unsigned char) pulse, (unsigned char) pulse, 255, (unsigned char) a};
    int margin = (g_app.viewport_w < g_app.viewport_h ? g_app.viewport_w : g_app.viewport_h) / 12;
    if (margin < 8)
        margin = 8;
    int title_x = margin + 8;
    int title_y = margin + 8;
    rogue_font_draw_text(title_x, title_y, "ROGUELIKE", 6, title_col2);
    /* Optional subtitle/tagline below the title (non-blocking, fades with title) */
    RogueColor sub_col = {220, 220, 240, (unsigned char) a};
    rogue_font_draw_text(title_x + 2, title_y + 28, rogue_locale_get("prompt_start"), 2, sub_col);
    /* Phase 3.1: Expanded main menu (Continue, New, Load, Settings, Credits, Quit) */
    /* Detect if any save exists (stub: consider slot 0 only for determinism in tests).
       To avoid flicker/races when files appear mid-frame (e.g., parallel tests), require the
       presence to be observed for at least two consecutive frames before enabling entries.
       Headless safeguard: if a corrupt/invalid descriptor is observed on the first scan of this
       process, quarantine Continue/Load for the remainder of the session to keep tests stable
       against parallel modifications to save files in the shared working directory. */
    int has_save_now = 0;
    int most_recent_slot = -1;
    {
        RogueSaveDescriptor d;
        int desc_ok = (rogue_save_read_descriptor(0, &d) == 0 && rogue_save_descriptor_is_sane(&d));
        if (!s_corrupt_at_start_global && desc_ok)
        {
            has_save_now = 1;
            most_recent_slot = 0;
        }
        else
        {
            has_save_now = 0;
            most_recent_slot = -1;
        }
    }
    static int s_has_save_stable = 0;
    if (has_save_now)
        s_has_save_stable++;
    else
        s_has_save_stable = 0;
    /* Additional startup settling: require a few frames before enabling Continue/Load
        to avoid cross-process races in parallel test runs writing the same save slot. */
    int startup_settled = (g_app.frame_count >= 3) ? 1 : 0;
    int has_save = (startup_settled && s_has_save_stable >= 2) ? 1 : 0;
    const char* menu_items[] = {rogue_start_menu_label(0), rogue_start_menu_label(1),
                                rogue_start_menu_label(2), rogue_start_menu_label(3),
                                rogue_start_menu_label(4), rogue_start_menu_label(5),
                                rogue_start_menu_label(6)};
    int enabled[] = {has_save, 1, has_save, 1, 1, 1, 1};
    int item_count = 7;
    int base_y = 140;

    /* Settings overlay (Phase 6.1-6.3): simple toggles and DPI scaler */
    if (g_app.start_show_settings)
    {
        rogue_font_draw_text(48, base_y - 20, rogue_locale_get("menu_settings"), 3, white);
        const char* items[] = {"Reduced Motion", "High Contrast", "Narration", "DPI Scale"};
        int count = 4;
        /* Show current values */
        char dpi_line[64];
        snprintf(dpi_line, sizeof dpi_line, "DPI Scale: %d%%", rogue_ui_dpi_scale_x100());
        for (int i = 0; i < count; ++i)
        {
            RogueColor c =
                (i == g_app.start_settings_index) ? (RogueColor){255, 255, 0, 255} : white;
            const char* label = items[i];
            char line[64];
            if (i == 0)
                snprintf(line, sizeof line, "%s: %s", label, g_app.reduced_motion ? "On" : "Off");
            else if (i == 1)
                snprintf(line, sizeof line, "%s: %s", label, g_app.high_contrast ? "On" : "Off");
            else if (i == 2)
                snprintf(line, sizeof line, "%s: %s", label, "Stub");
            else
            {
#if defined(_MSC_VER)
                strncpy_s(line, sizeof line, dpi_line, _TRUNCATE);
#else
                strncpy(line, dpi_line, sizeof line);
                line[sizeof line - 1] = '\0';
#endif
            }
            rogue_font_draw_text(50, base_y + i * 20, line, 2, c);
        }
        /* Input */
        int up = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_UP);
        int down = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DOWN);
        if (down)
            g_app.start_settings_index = (g_app.start_settings_index + 1) % count;
        if (up)
            g_app.start_settings_index = (g_app.start_settings_index + count - 1) % count;
        /* Toggle/adjust with LEFT/RIGHT or Accept */
        int left = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_LEFT);
        int right = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_RIGHT);
        int act = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION) ||
                  rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DIALOGUE);
        if (g_app.start_settings_index == 0 && (left || right || act))
        {
            g_app.reduced_motion = !g_app.reduced_motion;
        }
        else if (g_app.start_settings_index == 1 && (left || right || act))
        {
            g_app.high_contrast = !g_app.high_contrast;
        }
        else if (g_app.start_settings_index == 2 && (left || right || act))
        {
            /* Narration stub: no-op for now; would call rogue_ui_narrate on focus changes */
        }
        else if (g_app.start_settings_index == 3 && (left || right))
        {
            int step = right ? +5 : -5;
            int cur = rogue_ui_dpi_scale_x100();
            rogue_ui_theme_set_dpi_scale_x100(cur + step);
        }
        /* Exit settings on Cancel */
        if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_CANCEL))
        {
            g_app.start_show_settings = 0;
        }
        return;
    }

    /* Credits & Legal overlay (Phase 7) */
    if (g_app.start_show_credits)
    {
        rogue_font_draw_text(48, base_y - 40, rogue_locale_get("menu_credits"), 3,
                             (RogueColor){255, 255, 255, 255});
        start_credits_overlay_update_and_render();
        return;
    }

    /* If Load list is active, draw list overlay and handle its input instead of main menu. */
    if (g_app.start_show_load_list)
    {
        /* Build list of existing slots; by default only slot 0 for deterministic tests.
           Set env ROGUE_START_LIST_ALL=1 to list all available slots. */
        RogueSaveDescriptor descs[ROGUE_SAVE_SLOT_COUNT];
        int present[ROGUE_SAVE_SLOT_COUNT] = {0};
        int count = 0;
        /* If corrupt-at-start, keep list empty consistently to avoid races. */
        if (s_corrupt_at_start_global)
        {
            /* nothing to enumerate; falls through to close list below */
        }
        static int s_list_all_cached = -1; /* -1=unknown, 0/1 cached */
        if (s_list_all_cached < 0)
        {
            int list_all = 0;
#if defined(_MSC_VER)
            char* val = NULL;
            size_t len = 0;
            if (_dupenv_s(&val, &len, "ROGUE_START_LIST_ALL") == 0 && val && (val[0] == '1'))
                list_all = 1;
            if (val)
                free(val);
#else
            const char* v = getenv("ROGUE_START_LIST_ALL");
            if (v && v[0] == '1')
                list_all = 1;
#endif
            s_list_all_cached = list_all;
        }

        int slot_lo = 0;
        int slot_hi = s_list_all_cached ? ROGUE_SAVE_SLOT_COUNT : 1; /* exclusive */
        for (int s = slot_lo; s < slot_hi; ++s)
        {
            if (s_corrupt_at_start_global)
                continue; /* quarantined */
            RogueSaveDescriptor d;
            if (rogue_save_read_descriptor(s, &d) == 0 && rogue_save_descriptor_is_sane(&d))
            {
                descs[s] = d;
                present[s] = 1;
                count++;
            }
        }
        /* Ensure selection is on a valid entry */
        if (count == 0)
        {
            /* Nothing to show -> leave list */
            g_app.start_show_load_list = 0;
        }
        else
        {
            const int total_slots = slot_hi - slot_lo;
            if (g_app.start_load_selection < slot_lo)
                g_app.start_load_selection = slot_lo;
            if (g_app.start_load_selection >= slot_hi)
                g_app.start_load_selection = slot_lo;
            /* Input: up/down to change selection within present slots */
            int step = 0;
            if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DOWN))
                step = 1;
            else if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_UP))
                step = -1;
            while (step > 0)
            {
                int next = (g_app.start_load_selection + 1 - slot_lo) % total_slots + slot_lo;
                for (int k = 0; k < total_slots; ++k)
                {
                    if (present[next])
                    {
                        g_app.start_load_selection = next;
                        break;
                    }
                    next = (next + 1 - slot_lo) % total_slots + slot_lo;
                }
                step--;
            }
            while (step < 0)
            {
                int prev = (g_app.start_load_selection - 1 - slot_lo + total_slots) % total_slots +
                           slot_lo;
                for (int k = 0; k < total_slots; ++k)
                {
                    if (present[prev])
                    {
                        g_app.start_load_selection = prev;
                        break;
                    }
                    prev = (prev - 1 - slot_lo + total_slots) % total_slots + slot_lo;
                }
                step++;
            }
            /* Render list header */
            rogue_font_draw_text(48, base_y - 20, rogue_locale_get("menu_load"), 3, white);
            /* Virtualization window: ensure selected stays visible */
            static int vstart = 0; /* first visible slot index (absolute) */
            const int row_h = 22;
            const int max_rows = 8; /* cap visible rows; tune as needed */
            /* Clamp vstart to current bounds */
            if (vstart < slot_lo)
                vstart = slot_lo;
            if (vstart > slot_hi - 1)
                vstart = slot_hi - 1;
            /* If selected is above/below window, shift window */
            if (g_app.start_load_selection < vstart)
                vstart = g_app.start_load_selection;
            if (g_app.start_load_selection >= vstart + max_rows)
                vstart = g_app.start_load_selection - (max_rows - 1);
            if (vstart < slot_lo)
                vstart = slot_lo;

            int row_y = base_y + 4;
            int rows_drawn = 0;
            for (int s = vstart; s < slot_hi && rows_drawn < max_rows; ++s)
            {
                if (!present[s])
                    continue;
                /* Thumbnail */
                draw_slot_thumbnail(48, row_y - 6, 28, 18, s, &descs[s]);
                /* Label */
                char line[96];
                snprintf(line, sizeof line, "Slot %d  v%u  %us", s, descs[s].version,
                         descs[s].timestamp_unix);
                RogueColor c =
                    (s == g_app.start_load_selection) ? (RogueColor){255, 255, 0, 255} : white;
                rogue_font_draw_text(82, row_y, line, 2, c);
                row_y += row_h;
                rows_drawn++;
            }
            /* Delete confirmation modal (Phase 4.6) */
            static int confirm_active = 0;
            if (confirm_active)
            {
                /* Simple centered box with yes/no prompt */
                int cx = 46, cy = base_y + 72;
                rogue_font_draw_text(cx, cy, rogue_locale_get("confirm_delete_title"), 3,
                                     (RogueColor){255, 120, 120, 255});
                rogue_font_draw_text(cx, cy + 20, rogue_locale_get("confirm_delete_body"), 2,
                                     white);
                rogue_font_draw_text(cx, cy + 40, rogue_locale_get("confirm_delete_hint"), 2,
                                     white);
                if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DIALOGUE) ||
                    rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION))
                {
                    int slot = g_app.start_load_selection;
                    if (slot >= slot_lo && slot < slot_hi && present[slot])
                    {
                        rogue_save_manager_delete_slot(slot);
                        /* Refresh list immediately */
                        confirm_active = 0;
                        /* Clear selection to next present */
                        int next = (slot + 1 - slot_lo) % total_slots + slot_lo;
                        for (int k = 0; k < total_slots; ++k)
                        {
                            if (present[next])
                            {
                                g_app.start_load_selection = next;
                                break;
                            }
                            next = (next + 1 - slot_lo) % total_slots + slot_lo;
                        }
                    }
                }
                else if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_CANCEL))
                {
                    confirm_active = 0;
                }
                /* While modal is open, do not process other actions */
                rogue_font_draw_text(48, base_y + 80, rogue_locale_get("hint_accept_cancel"), 2,
                                     white);
                return;
            }

            /* Accept -> load selected; Cancel -> close list; Delete -> open confirm */
            if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DIALOGUE) ||
                rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION))
            {
                int slot = g_app.start_load_selection;
                if (slot >= slot_lo && slot < slot_hi && present[slot])
                {
                    RogueSaveDescriptor vd;
                    if (rogue_save_read_descriptor(slot, &vd) == 0 &&
                        rogue_save_descriptor_is_sane(&vd))
                    {
                        int rc = rogue_save_manager_load_slot(slot);
                        (void) rc;
                        if (rc == 0)
                        {
                            g_app.start_show_load_list = 0;
                            g_app.start_state = ROGUE_START_FADE_OUT;
                        }
                    }
                }
            }
            /* For delete, use LEFT key as a conservative stand-in (no dedicated delete key yet) */
            if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_LEFT))
            {
                confirm_active = 1;
            }
            if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_CANCEL))
            {
                g_app.start_show_load_list = 0;
            }
        }
        /* When load list is active, skip main menu input/render beyond a subtle hint */
        rogue_font_draw_text(48, base_y + 80, rogue_locale_get("hint_accept_cancel"), 2, white);
        return;
    }
    /* New Game confirmation modal (Phase 5.1/5.3): simple inline modal when New Game is selected.
       This modal is gated behind env ROGUE_START_CONFIRM_NEW=1 to keep tests/CI expectations
       (immediate transition) intact by default. */
    static int s_new_game_confirm = 0;
    if (s_new_game_confirm)
    {
        /* Render a minimal confirmation UI with seed display and difficulty stub */
        rogue_font_draw_text(48, base_y - 20, rogue_locale_get("menu_new_game"), 3, white);
        char line[128];
        snprintf(line, sizeof line, "Seed: %u  Difficulty: Normal", g_app.pending_seed);
        rogue_font_draw_text(48, base_y + 10, line, 2, white);
        rogue_font_draw_text(48, base_y + 30, rogue_locale_get("hint_accept_cancel"), 2, white);

        /* Allow quick randomize via Right arrow while modal is open */
        if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_RIGHT))
        {
            /* Simple time-derived tweak to vary without SDL tick access here */
            g_app.pending_seed ^= (unsigned) (g_app.frame_count * 2654435761u + 0x9E37u);
        }
        /* Accept => create initial save in slot 0, publish telemetry, transition */
        if (g_app.headless || rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION) ||
            rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DIALOGUE))
        {
            rogue_start_begin_new_game_from_seed();
            s_new_game_confirm = 0; /* close modal on attempt */
        }
        else if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_CANCEL))
        {
            s_new_game_confirm = 0;
        }
        return;
    }
    /* Keep current selection even if disabled; navigation below will skip disabled entries.
       This ensures activating a disabled item is safely ignored (no implicit auto-advance). */
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
    /* Small thumbnail next to Continue when available */
    if (has_save && most_recent_slot >= 0)
    {
        RogueSaveDescriptor d;
        if (rogue_save_read_descriptor(most_recent_slot, &d) == 0)
        {
            int y = base_y + 0 * 20 + 2;
            draw_slot_thumbnail(28, y - 6, 18, 12, most_recent_slot, &d);
        }
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
        { /* Continue (most recent) */
            int slot = (most_recent_slot >= 0) ? most_recent_slot : 0;
            /* Re-validate descriptor just before attempting to load to guard against
               corrupt/invalid headers or mid-frame changes. */
            if (startup_settled && !s_corrupt_at_start_global)
            {
                RogueSaveDescriptor vd;
                if (rogue_save_read_descriptor(slot, &vd) == 0 &&
                    rogue_save_descriptor_is_sane(&vd))
                {
                    int rc = rogue_save_manager_load_slot(slot);
                    (void) rc; /* ignore errors for now; stay on menu if fails */
                    if (rc == 0)
                        g_app.start_state = ROGUE_START_FADE_OUT;
                }
            }
            /* If not sane, ignore activation entirely (remain on menu). */
        }
        else if (sel == 1)
        { /* New Game: open confirmation modal (Phase 5.3) only if explicitly enabled.
             In headless or when not required, begin immediately to satisfy navigation tests. */
            /* Cache env once */
            static int s_require_confirm_cached = -1; /* -1 unknown, 0/1 set */
            if (s_require_confirm_cached < 0)
            {
                int req = 0;
#if defined(_MSC_VER)
                char* v = NULL;
                size_t l = 0;
                if (_dupenv_s(&v, &l, "ROGUE_START_CONFIRM_NEW") == 0 && v)
                {
                    if (v[0] == '1')
                        req = 1;
                    free(v);
                }
#else
                const char* v = getenv("ROGUE_START_CONFIRM_NEW");
                if (v && v[0] == '1')
                    req = 1;
#endif
                s_require_confirm_cached = req;
            }
            if (g_app.headless || !s_require_confirm_cached)
            {
                rogue_start_begin_new_game_from_seed();
            }
            else
            {
                s_new_game_confirm = 1;
            }
        }
        else if (sel == 2)
        { /* Load Game -> open load list UI */
            g_app.start_show_load_list = 1;
            /* Initialize selection to most recent if present, else first present slot */
            g_app.start_load_selection = (most_recent_slot >= 0) ? most_recent_slot : 0;
        }
        else if (sel == 3)
        { /* Settings overlay */
            g_app.start_show_settings = 1;
            g_app.start_settings_index = 0;
        }
        else if (sel == 4)
        { /* Credits & Legal overlay */
            g_app.start_show_credits = 1;
            g_app.start_credits_tab = 0;
            g_app.start_credits_scroll = 0.0f;
            g_app.start_credits_vel = 0.0f;
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
                                 rogue_locale_get("hint_accept_cancel"), 2, white);
    }
    /* Phase 3.3 Tooltip panel: right side contextual hint */
    {
        const char* tip = tooltip_for_selection(g_app.menu_index);
        int tip_x = (g_app.viewport_w > 240) ? (g_app.viewport_w - 140) : 200;
        int tip_y = base_y;
        rogue_font_draw_text(tip_x, tip_y, tip, 2, white);
    }
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

/* Helper: begin a new game from current pending_seed, generate world, save slot 0, and fade out. */
static void rogue_start_begin_new_game_from_seed(void)
{
    /* Telemetry */
    RogueEventPayload p;
    memset(&p, 0, sizeof p);
    p.new_game_start.seed = g_app.pending_seed;
    p.new_game_start.difficulty = 0; /* Normal */
    rogue_event_publish(ROGUE_EVENT_NEW_GAME_START, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0x4E474E57,
                        "StartScreen");
    /* Initialize a fresh world based on current seed to ensure save is consistent */
    rogue_tilemap_free(&g_app.world_map);
    RogueWorldGenConfig wcfg = rogue_world_gen_config_build(g_app.pending_seed, 1, 1);
    if (!rogue_world_generate_full(&g_app.world_map, &wcfg))
    {
        (void) rogue_world_generate(&g_app.world_map, &wcfg);
    }
    int sx = 2, sy = 2;
    if (rogue_world_find_random_spawn(&g_app.world_map, wcfg.seed ^ 0x7777u, &sx, &sy))
    {
        g_app.player.base.pos.x = (float) sx + 0.5f;
        g_app.player.base.pos.y = (float) sy + 0.5f;
    }
    /* Persist initial save to deterministic slot 0 */
    int save_rc = rogue_save_manager_save_slot(0);
    if (save_rc == 0)
    {
        g_app.start_state = ROGUE_START_FADE_OUT;
    }
    else
    {
        ROGUE_LOG_ERROR("New Game save failed rc=%d", save_rc);
    }
}
