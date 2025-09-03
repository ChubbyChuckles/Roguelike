#include "../../audio_vfx/effects.h"
#include "../../core/app/app_state.h"
#include "../../core/audio_vfx/audiovfx_debug.h"
#include "../overlay_core.h"
#include "../overlay_icon.h"
#include "../overlay_input.h"
#include "../overlay_theme.h"
#include "../widgets/overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_audiovfx(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("audiovfx", "Audio / VFX", 10, 590, 380))
        return;
    /* Simple inputs: audio id, vfx id, and spawn at cursor */
    static char audio_id[32] = "click";
    static char vfx_id[32] = "SPARKLE";
    overlay_input_text("Audio ID", audio_id, sizeof audio_id);
    overlay_input_text("VFX ID", vfx_id, sizeof vfx_id);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_icon_button("Play Sound", OVERLAY_ICON_PLAY))
            (void) rogue_audiovfx_debug_play(audio_id);
        overlay_next_column();
        if (overlay_icon_button("Spawn VFX @ Cursor", OVERLAY_ICON_PLAY))
        {
            const OverlayInputState* in = overlay_input_get();
            (void) rogue_audiovfx_debug_spawn_at_cursor(vfx_id, in->mouse_x, in->mouse_y);
        }
        overlay_columns_end();
    }

    /* Positional audition: enable, radius, listener follow, and Play @ Cursor */
    static int pos_enable = 0;
    static float falloff_radius = 6.0f; /* tiles */
    static int listener_follow_player = 1;
    static int show_falloff_ring = 0;
    int pos_changed = 0;
    if (overlay_checkbox("Enable Positional Audio", &pos_enable))
        pos_changed = 1;
    if (overlay_slider_float("Falloff Radius (tiles)", &falloff_radius, 1.0f, 24.0f))
        pos_changed = 1;
    if (pos_changed)
        rogue_audiovfx_debug_set_positional(pos_enable, falloff_radius);
    if (overlay_checkbox("Listener follows player", &listener_follow_player))
    {
        /* no immediate action; updated below per-frame */
        (void) 0;
    }
    overlay_checkbox("Show falloff ring gizmo", &show_falloff_ring);

    /* Update listener from player each frame if requested */
    if (listener_follow_player)
    {
        float lx = g_app.player.base.pos.x;
        float ly = g_app.player.base.pos.y;
        rogue_audio_set_listener(lx, ly);
    }

    /* Play @ Cursor (with world coords so attenuation applies) */
    if (overlay_icon_button("Play Sound @ Cursor", OVERLAY_ICON_PLAY))
    {
        const OverlayInputState* in = overlay_input_get();
        /* Convert cursor (screen px) to world tiles */
        int ts = g_app.tile_size ? g_app.tile_size : 32;
        float wx = (g_app.cam_x + (float) in->mouse_x) / (float) ts;
        float wy = (g_app.cam_y + (float) in->mouse_y) / (float) ts;
        RogueEffectEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.type = (uint8_t) ROGUE_FX_AUDIO_PLAY;
        ev.priority = (uint8_t) ROGUE_FX_PRI_UI;
#if defined(_MSC_VER)
        strncpy_s(ev.id, sizeof ev.id, audio_id, _TRUNCATE);
#else
        strncpy(ev.id, audio_id, sizeof ev.id - 1);
        ev.id[sizeof ev.id - 1] = '\0';
#endif
        ev.repeats = 1;
        ev.x = wx;
        ev.y = wy;
        (void) rogue_fx_emit(&ev);
    }

    /* Mixer controls */
    static float master = 1.0f;
    static float cat_sfx = 1.0f;
    static float cat_ui = 1.0f;
    static int mute = 0;
    if (overlay_slider_float("Master", &master, 0.0f, 1.0f))
        rogue_audiovfx_debug_set_master(master);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_slider_float("SFX", &cat_sfx, 0.0f, 1.0f))
            rogue_audiovfx_debug_set_category(0, cat_sfx);
        overlay_next_column();
        if (overlay_slider_float("UI", &cat_ui, 0.0f, 1.0f))
            rogue_audiovfx_debug_set_category(1, cat_ui);
        overlay_columns_end();
    }
    if (overlay_checkbox("Mute", &mute))
        rogue_audiovfx_debug_set_mute(mute);

    /* VFX perf controls */
    static float perf = 1.0f;
    static int soft_cap = 0;
    static int hard_cap = 0;
    if (overlay_slider_float("VFX Perf Scale", &perf, 0.1f, 1.0f))
        rogue_audiovfx_debug_set_perf(perf);
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_slider_int("Soft Budget", &soft_cap, 0, 2000))
            rogue_audiovfx_debug_set_budgets(soft_cap, hard_cap);
        overlay_next_column();
        if (overlay_slider_int("Hard Budget", &hard_cap, 0, 4000))
            rogue_audiovfx_debug_set_budgets(soft_cap, hard_cap);
        overlay_columns_end();
    }

    /* Quick attenuation curve (ASCII) preview for current audio id */
    {
        char line[96];
        overlay_label("Attenuation Preview (distance -> effective gain)");
        /* Sample 5 distances: 0%, 25%, 50%, 75%, 100% of radius (or 1.0 if disabled) */
        int samples = 5;
        for (int i = 0; i < samples; ++i)
        {
            float frac = (float) i / (float) (samples - 1);
            float d = pos_enable ? (falloff_radius * frac) : 0.0f;
            /* Position a test source due east of listener */
            float lx = g_app.player.base.pos.x;
            float ly = g_app.player.base.pos.y;
            float sx = lx + d;
            float sy = ly;
            float eff = rogue_audio_debug_effective_gain(audio_id, 1u, sx, sy);
            if (eff < 0.0f)
                eff = 0.0f;
            if (eff > 1.0f)
                eff = 1.0f;
            int bars = (int) (eff * 24.0f + 0.5f);
            if (bars < 0)
                bars = 0;
            if (bars > 24)
                bars = 24;
            int pos = 0;
            pos += snprintf(line + pos, sizeof line - (size_t) pos, "d=%4.1f ", d);
            line[pos++] = '[';
            for (int b = 0; b < 24 && pos < (int) sizeof line - 2; ++b)
                line[pos++] = (b < bars) ? '*' : ' ';
            line[pos++] = ']';
            line[pos] = '\0';
            overlay_label(line);
        }
    }

    /* Stats readout */
    struct RogueVfxFrameStats st = {0};
    rogue_audiovfx_debug_get_last_stats(&st);
    char buf[160];
    snprintf(buf, sizeof buf,
             "parts: %d  inst: %d  spawned(core:%d trail:%d) culled(s:%d h:%d p:%d)",
             st.active_particles, st.active_instances, st.spawned_core, st.spawned_trail,
             st.culled_soft, st.culled_hard, st.culled_pacing);
    overlay_label(buf);

    /* Optional: draw a simple falloff ring gizmo around the listener (player) */
#ifdef ROGUE_HAVE_SDL
    if (show_falloff_ring && g_app.renderer && falloff_radius > 0.0f)
    {
        const OverlayTheme* th = overlay_theme_get();
        int ts = g_app.tile_size ? g_app.tile_size : 32;
        float px = g_app.player.base.pos.x * (float) ts - g_app.cam_x;
        float py = g_app.player.base.pos.y * (float) ts - g_app.cam_y;
        float r_px = falloff_radius * (float) ts;
        /* Use theme accent color for ring */
        if (th)
            SDL_SetRenderDrawColor(g_app.renderer, th->accent_2.r, th->accent_2.g, th->accent_2.b,
                                   160);
        else
            SDL_SetRenderDrawColor(g_app.renderer, 180, 180, 180, 160);
        /* Midpoint circle approximation */
        int segments = 64;
        float prev_x = px + r_px;
        float prev_y = py;
        for (int s = 1; s <= segments; ++s)
        {
            float t0 = (float) (2.0 * 3.14159265358979323846 * (s - 1) / segments);
            float t1 = (float) (2.0 * 3.14159265358979323846 * s / segments);
            float x0 = px + r_px * (float) cos(t0);
            float y0 = py + r_px * (float) sin(t0);
            float x1 = px + r_px * (float) cos(t1);
            float y1 = py + r_px * (float) sin(t1);
            SDL_RenderDrawLine(g_app.renderer, (int) x0, (int) y0, (int) x1, (int) y1);
        }
    }
#endif

    overlay_end_panel();
}

void rogue_overlay_register_panel_audiovfx(void)
{
    overlay_register_panel("audiovfx", "Audio / VFX", panel_audiovfx, NULL);
}

#endif
