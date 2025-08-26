#include "start_screen.h"
#include "../core/persistence/save_manager.h" /* for simple Continue/Load stubs (slot 0) */
#include "../graphics/font.h"
#include "../graphics/sprite.h"
#include "../input/input.h"
#include "../util/log.h"
#include "game_loop.h"
#include "localization.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rogue_start_screen_active(void) { return g_app.show_start_screen; }

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
    /* Phase 2.5: simple parallax stars/particles overlay (unaffected by reduced motion) */
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

void rogue_start_screen_update_and_render(void)
{
    if (!g_app.show_start_screen)
        return;
    g_app.title_time += g_app.dt;
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
    /* Detect if any save exists (stub: consider slot 0 only for determinism in tests) */
    int has_save = 0;
    int most_recent_slot = -1;
    {
        RogueSaveDescriptor d;
        if (rogue_save_read_descriptor(0, &d) == 0)
        {
            has_save = 1;
            most_recent_slot = 0;
        }
    }
    const char* menu_items[] = {rogue_start_menu_label(0), rogue_start_menu_label(1),
                                rogue_start_menu_label(2), rogue_start_menu_label(3),
                                rogue_start_menu_label(4), rogue_start_menu_label(5),
                                rogue_start_menu_label(6)};
    int enabled[] = {has_save, 1, has_save, 1, 1, 1, 1};
    int item_count = 7;
    int base_y = 140;

    /* If Load list is active, draw list overlay and handle its input instead of main menu. */
    if (g_app.start_show_load_list)
    {
        /* Build a simple list of existing slots with their descriptors (slot 0 only) */
        RogueSaveDescriptor descs[ROGUE_SAVE_SLOT_COUNT];
        int present[ROGUE_SAVE_SLOT_COUNT] = {0};
        int count = 0;
        {
            RogueSaveDescriptor d;
            if (rogue_save_read_descriptor(0, &d) == 0)
            {
                descs[0] = d;
                present[0] = 1;
                count = 1;
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
            if (g_app.start_load_selection < 0)
                g_app.start_load_selection = 0;
            if (g_app.start_load_selection >= 1)
                g_app.start_load_selection = 0;
            /* Input: up/down to change selection within present slots */
            int step = 0;
            if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DOWN))
                step = 1;
            else if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_UP))
                step = -1;
            while (step > 0)
            {
                int next = (g_app.start_load_selection + 1) % 1;
                for (int k = 0; k < 1; ++k)
                {
                    if (present[next])
                    {
                        g_app.start_load_selection = next;
                        break;
                    }
                    next = (next + 1) % 1;
                }
                step--;
            }
            while (step < 0)
            {
                int prev = (g_app.start_load_selection + 1 - 1) % 1;
                for (int k = 0; k < 1; ++k)
                {
                    if (present[prev])
                    {
                        g_app.start_load_selection = prev;
                        break;
                    }
                    prev = (prev + 1 - 1) % 1;
                }
                step++;
            }
            /* Render list header */
            rogue_font_draw_text(48, base_y - 20, rogue_locale_get("menu_load"), 3, white);
            int row_y = base_y + 4;
            for (int s = 0; s < 1; ++s)
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
                row_y += 22;
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
                    if (slot == 0 && present[slot])
                    {
                        rogue_save_manager_delete_slot(slot);
                        /* Refresh list immediately */
                        confirm_active = 0;
                        /* Clear selection to next present */
                        int next = (slot + 1) % 1;
                        for (int k = 0; k < 1; ++k)
                        {
                            if (present[next])
                            {
                                g_app.start_load_selection = next;
                                break;
                            }
                            next = (next + 1) % 1;
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
                if (slot == 0 && present[slot])
                {
                    RogueSaveDescriptor vd;
                    if (rogue_save_read_descriptor(slot, &vd) == 0)
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
            RogueSaveDescriptor vd;
            if (rogue_save_read_descriptor(slot, &vd) == 0)
            {
                int rc = rogue_save_manager_load_slot(slot);
                (void) rc; /* ignore errors for now; stay on menu if fails */
                if (rc == 0)
                    g_app.start_state = ROGUE_START_FADE_OUT;
            }
        }
        else if (sel == 1)
        { /* New Game */
            g_app.start_state = ROGUE_START_FADE_OUT;
        }
        else if (sel == 2)
        { /* Load Game -> open load list UI */
            g_app.start_show_load_list = 1;
            /* Initialize selection to most recent if present, else first present slot */
            g_app.start_load_selection = (most_recent_slot >= 0) ? most_recent_slot : 0;
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
