#include "../../audio_vfx/effects.h"
#include "../../core/audio_vfx/audiovfx_debug.h"
#include "../overlay_core.h"
#include "../overlay_input.h"
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
        if (overlay_button("Play Sound"))
            (void) rogue_audiovfx_debug_play(audio_id);
        overlay_next_column();
        if (overlay_button("Spawn VFX @ Cursor"))
        {
            const OverlayInputState* in = overlay_input_get();
            (void) rogue_audiovfx_debug_spawn_at_cursor(vfx_id, in->mouse_x, in->mouse_y);
        }
        overlay_columns_end();
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

    /* Stats readout */
    struct RogueVfxFrameStats st = {0};
    rogue_audiovfx_debug_get_last_stats(&st);
    char buf[160];
    snprintf(buf, sizeof buf,
             "parts: %d  inst: %d  spawned(core:%d trail:%d) culled(s:%d h:%d p:%d)",
             st.active_particles, st.active_instances, st.spawned_core, st.spawned_trail,
             st.culled_soft, st.culled_hard, st.culled_pacing);
    overlay_label(buf);

    overlay_end_panel();
}

void rogue_overlay_register_panel_audiovfx(void)
{
    overlay_register_panel("audiovfx", "Audio / VFX", panel_audiovfx, NULL);
}

#endif
