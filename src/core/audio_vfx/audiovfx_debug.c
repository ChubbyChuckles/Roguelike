#include "audiovfx_debug.h"
#include "../../audio_vfx/effects.h"
#include "../app/app_state.h"

int rogue_audiovfx_debug_play(const char* audio_id)
{
    if (!audio_id || !audio_id[0])
        return -1;
    rogue_audio_play_by_id(audio_id);
    return 0;
}

int rogue_audiovfx_debug_spawn_at(const char* vfx_id, float x, float y)
{
    if (!vfx_id || !vfx_id[0])
        return -1;
    return rogue_vfx_spawn_by_id(vfx_id, x, y);
}

int rogue_audiovfx_debug_spawn_at_cursor(const char* vfx_id, int screen_x, int screen_y)
{
    if (!vfx_id || !vfx_id[0])
        return -1;
    int world_space = 1;
    RogueVfxLayer layer;
    uint32_t life_ms;
    if (rogue_vfx_registry_get(vfx_id, &layer, &life_ms, &world_space) != 0)
        return -2;
    float x = (float) screen_x;
    float y = (float) screen_y;
    if (world_space)
    {
        /* Convert from screen pixels to world units using camera and tile size. */
        int ts = g_app.tile_size ? g_app.tile_size : 32;
        float world_px_x = g_app.cam_x + (float) screen_x;
        float world_px_y = g_app.cam_y + (float) screen_y;
        x = world_px_x / (float) ts;
        y = world_px_y / (float) ts;
    }
    return rogue_vfx_spawn_by_id(vfx_id, x, y);
}

int rogue_audiovfx_debug_validate_audio(const char* audio_id)
{
    if (!audio_id || !audio_id[0])
        return -1;
    char buf[4];
    return rogue_audio_registry_get_path(audio_id, buf, sizeof buf);
}

int rogue_audiovfx_debug_validate_vfx(const char* vfx_id)
{
    if (!vfx_id || !vfx_id[0])
        return -1;
    RogueVfxLayer layer;
    uint32_t life_ms;
    int world_space = 0;
    return rogue_vfx_registry_get(vfx_id, &layer, &life_ms, &world_space);
}

void rogue_audiovfx_debug_set_master(float gain) { rogue_audio_mixer_set_master(gain); }
void rogue_audiovfx_debug_set_category(int cat, float gain)
{
    if (cat < 0)
        cat = 0;
    if (cat > 3)
        cat = 3;
    rogue_audio_mixer_set_category((RogueAudioCategory) cat, gain);
}
void rogue_audiovfx_debug_set_mute(int mute) { rogue_audio_mixer_set_mute(mute); }
void rogue_audiovfx_debug_set_positional(int enable, float falloff_radius)
{
    rogue_audio_enable_positional(enable);
    if (falloff_radius > 0)
        rogue_audio_set_falloff_radius(falloff_radius);
}

void rogue_audiovfx_debug_set_perf(float scale) { rogue_vfx_set_perf_scale(scale); }
void rogue_audiovfx_debug_set_budgets(int soft_cap, int hard_cap)
{
    rogue_vfx_set_spawn_budgets(soft_cap, hard_cap);
}

void rogue_audiovfx_debug_get_last_stats(struct RogueVfxFrameStats* out)
{
    rogue_vfx_profiler_get_last(out);
}
