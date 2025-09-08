/* asset_browser_audio_detail.c - extracted from panels_asset_browser.c */
#include "asset_browser_audio_detail.h"
#include "../../asset/asset_manager.h"
#include "../../util/asset_dep.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "asset_browser_state.h"
#include <stdio.h>
#include <string.h>

#if defined(ROGUE_HAVE_SDL_MIXER)
#include <SDL_mixer.h>
#endif

#define g_ab_state (*rogue_asset_browser_state())

void rogue_asset_browser_draw_audio_detail(const struct RogueAssetAudio* sel_audio,
                                           const struct RogueAssetManager* m)
{
    if (!sel_audio || !m)
        return;
    char line[256];
    snprintf(line, sizeof line, "Selected Audio: id=%s ref=%u fail=%d loaded=%d", sel_audio->id,
             sel_audio->ref_count, sel_audio->load_failed ? 1 : 0, sel_audio->sdl_chunk ? 1 : 0);
    overlay_label(line);
    int audio_index = -1;
    for (uint32_t ai = 0; ai < m->audio_count; ++ai)
        if (&m->audio[ai] == sel_audio)
        {
            audio_index = (int) ai;
            break;
        }
    if (audio_index >= 0)
    {
        overlay_label("Tags:");
        const char* atags[8];
        int ac = rogue_asset_manager_list_audio_tags(audio_index, atags, 8);
        if (ac == 0)
            overlay_label("(none)");
        for (int ti = 0; ti < ac; ++ti)
            if (overlay_button(atags[ti]))
                rogue_asset_manager_remove_audio_tag(audio_index, atags[ti]);
        if (overlay_input_text("Add Tag", g_ab_state.tag_input, sizeof g_ab_state.tag_input))
        {
        }
        if (overlay_button("+Tag") && g_ab_state.tag_input[0])
        {
            rogue_asset_manager_add_audio_tag(audio_index, g_ab_state.tag_input);
            g_ab_state.tag_input[0] = '\0';
        }
    }
#if defined(ROGUE_HAVE_SDL_MIXER)
    static int g_ab_audio_channel = -1;
    if (g_ab_state.audio_volume <= 0)
        g_ab_state.audio_volume = 96;
    if (overlay_button("Play"))
    {
        int loops = g_ab_state.audio_loop ? -1 : 0;
        g_ab_audio_channel = Mix_PlayChannel(-1, (Mix_Chunk*) sel_audio->sdl_chunk, loops);
        if (g_ab_audio_channel >= 0)
            Mix_Volume(g_ab_audio_channel, g_ab_state.audio_volume);
    }
    if (overlay_button("Stop"))
    {
        if (g_ab_audio_channel >= 0)
        {
            Mix_HaltChannel(g_ab_audio_channel);
            g_ab_audio_channel = -1;
        }
    }
    overlay_checkbox("Loop", &g_ab_state.audio_loop);
    if (overlay_button("Vol+") && g_ab_state.audio_volume < 128)
    {
        g_ab_state.audio_volume += 8;
        if (g_ab_audio_channel >= 0)
            Mix_Volume(g_ab_audio_channel, g_ab_state.audio_volume);
    }
    if (overlay_button("Vol-") && g_ab_state.audio_volume > 0)
    {
        g_ab_state.audio_volume -= 8;
        if (g_ab_audio_channel >= 0)
            Mix_Volume(g_ab_audio_channel, g_ab_state.audio_volume);
    }
    snprintf(line, sizeof line, "Volume: %d", g_ab_state.audio_volume);
    overlay_label(line);
#endif
    {
        const char* dep_ids[32];
        int depc = rogue_asset_dep_get_deps(sel_audio->id, dep_ids, 32);
        if (depc > 0)
        {
            overlay_label("Deps:");
            for (int di = 0; di < depc; ++di)
                overlay_label(dep_ids[di]);
        }
    }
#if defined(ROGUE_HAVE_SDL_MIXER)
    {
        int audio_index2 = -1;
        for (uint32_t i = 0; i < m->audio_count; ++i)
            if (&m->audio[i] == sel_audio)
            {
                audio_index2 = (int) i;
                break;
            }
        static int lp_start_ms = 0;
        static int lp_end_ms = 0;
        static char lp_status[64];
        if (overlay_button("Load Loop Pts"))
        {
            uint32_t s, e;
            if (rogue_asset_manager_get_audio_loop_points(audio_index2, &s, &e))
            {
                lp_start_ms = (int) s;
                lp_end_ms = (int) e;
                snprintf(lp_status, sizeof lp_status, "Loaded %u-%u ms", s, e);
            }
            else
            {
                lp_start_ms = 0;
                lp_end_ms = 0;
                snprintf(lp_status, sizeof lp_status, "(none)");
            }
        }
        if (overlay_button("Start -10") && lp_start_ms >= 10)
            lp_start_ms -= 10;
        if (overlay_button("Start +10"))
            lp_start_ms += 10;
        if (overlay_button("End -10") && lp_end_ms >= 10)
            lp_end_ms -= 10;
        if (overlay_button("End +10"))
            lp_end_ms += 10;
        if (overlay_button("Apply Loop Pts"))
        {
            if (lp_end_ms > lp_start_ms && audio_index2 >= 0)
            {
                rogue_asset_manager_set_audio_loop_points(audio_index2, (uint32_t) lp_start_ms,
                                                          (uint32_t) lp_end_ms);
                snprintf(lp_status, sizeof lp_status, "Set %d-%d ms", lp_start_ms, lp_end_ms);
            }
            else
            {
                rogue_asset_manager_set_audio_loop_points(audio_index2, 0, 0);
                snprintf(lp_status, sizeof lp_status, "Disabled");
            }
        }
        snprintf(line, sizeof line, "Loop Start: %d ms", lp_start_ms);
        overlay_label(line);
        snprintf(line, sizeof line, "Loop End  : %d ms", lp_end_ms);
        overlay_label(line);
        if (lp_status[0])
            overlay_label(lp_status);
    }
#endif
}
