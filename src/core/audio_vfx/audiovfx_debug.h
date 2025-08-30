#ifndef ROGUE_AUDIOVFX_DEBUG_H
#define ROGUE_AUDIOVFX_DEBUG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Lightweight debug/testing helpers for Audio/VFX used by the overlay and unit tests. */

    /* Play a registered sound id immediately via the audio dispatcher. Returns 0 on success. */
    int rogue_audiovfx_debug_play(const char* audio_id);

    /* Spawn a VFX by id at the given coordinates. Returns 0 on success. */
    int rogue_audiovfx_debug_spawn_at(const char* vfx_id, float x, float y);

    /* Spawn a VFX by id at the given screen cursor position. If the VFX is world-space,
       the position is converted from screen pixels to world units using camera and tile size. */
    int rogue_audiovfx_debug_spawn_at_cursor(const char* vfx_id, int screen_x, int screen_y);

    /* Validate registry references; return 0 if present, <0 if missing. */
    int rogue_audiovfx_debug_validate_audio(const char* audio_id);
    int rogue_audiovfx_debug_validate_vfx(const char* vfx_id);

    /* Mixer and positional controls (thin wrappers). */
    void rogue_audiovfx_debug_set_master(float gain);
    void rogue_audiovfx_debug_set_category(int cat, float gain);
    void rogue_audiovfx_debug_set_mute(int mute);
    void rogue_audiovfx_debug_set_positional(int enable, float falloff_radius);

    /* VFX performance controls and stats. */
    void rogue_audiovfx_debug_set_perf(float scale);
    void rogue_audiovfx_debug_set_budgets(int soft_cap, int hard_cap);

    /* Forward-declare stats type to avoid including the full header here. */
    struct RogueVfxFrameStats;
    void rogue_audiovfx_debug_get_last_stats(struct RogueVfxFrameStats* out);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_AUDIOVFX_DEBUG_H */
