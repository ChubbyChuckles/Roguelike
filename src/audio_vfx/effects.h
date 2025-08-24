/* Minimal audio/VFX event bus (Phase 1+2 seed) */
#ifndef ROGUE_EFFECTS_H
#define ROGUE_EFFECTS_H

#include <stddef.h>
#include <stdint.h>

/* Effect type */
typedef enum RogueEffectEventType
{
    ROGUE_FX_AUDIO_PLAY = 1,
    ROGUE_FX_VFX_SPAWN = 2
} RogueEffectEventType;

/* Priority classes for deterministic ordering */
typedef enum RogueEffectPriority
{
    ROGUE_FX_PRI_CRITICAL = 0,
    ROGUE_FX_PRI_COMBAT = 1,
    ROGUE_FX_PRI_UI = 2,
    ROGUE_FX_PRI_AMBIENCE = 3
} RogueEffectPriority;

typedef struct RogueEffectEvent
{
    uint32_t emit_frame; /* producer frame index (optional; 0 if unknown) */
    uint32_t seq;        /* sequence id within frame (assigned by bus) */
    uint8_t priority;    /* RogueEffectPriority */
    uint8_t type;        /* RogueEffectEventType */
    uint16_t repeats;    /* frame compaction: number of identical events merged (>=1) */
    /* Payload: for audio, string id key (small fixed buffer); for vfx, params TBD */
    char id[24];
    float x, y; /* for VFX/world-space if needed */
} RogueEffectEvent;

/* Frame lifecycle */
void rogue_fx_frame_begin(uint32_t frame_index);
void rogue_fx_frame_end(void);

/* Emit event (deterministic ordering assigned internally). Returns 0 on success. */
int rogue_fx_emit(const RogueEffectEvent* ev);

/* Process and dispatch queued events (play audio, spawn vfx). Returns processed count. */
int rogue_fx_dispatch_process(void);

/* Stable digest for test/replay (updated when dispatch runs). */
uint32_t rogue_fx_get_frame_digest(void);

/* -------- Audio registry (Phase 2.1 minimal) -------- */

typedef enum RogueAudioCategory
{
    ROGUE_AUDIO_CAT_SFX = 0,
    ROGUE_AUDIO_CAT_UI = 1,
    ROGUE_AUDIO_CAT_AMBIENCE = 2,
    ROGUE_AUDIO_CAT_MUSIC = 3
} RogueAudioCategory;

/* Register or update a sound id -> path mapping with optional base gain [0..1]. */
int rogue_audio_registry_register(const char* id, const char* path, RogueAudioCategory cat,
                                  float base_gain);

/* Internal dispatch helper (used by dispatcher). */
void rogue_audio_play_by_id(const char* id);

/* Registry helpers (tests + tooling) */
int rogue_audio_registry_get_path(const char* id, char* out, size_t out_sz);
void rogue_audio_registry_clear(void);

/* Channel mixer (Phase 2.3) */
void rogue_audio_mixer_set_master(float gain); /* 0..1 */
float rogue_audio_mixer_get_master(void);
void rogue_audio_mixer_set_category(RogueAudioCategory cat, float gain); /* 0..1 */
float rogue_audio_mixer_get_category(RogueAudioCategory cat);
void rogue_audio_mixer_set_mute(int mute); /* non-zero = mute */
int rogue_audio_mixer_get_mute(void);

/* Positional audio stub (Phase 2.6) */
void rogue_audio_set_listener(float x, float y);
void rogue_audio_enable_positional(int enable); /* non-zero = enabled */
void rogue_audio_set_falloff_radius(float r);   /* >0 */

/* Debug/testing helper to compute effective gain scalar (base*category*master*atten)
    without requiring SDL_mixer. Repeats >= 1; (x,y) is event position. */
float rogue_audio_debug_effective_gain(const char* id, unsigned repeats, float x, float y);

/* -------- VFX subsystem (Phase 3 foundations) -------- */

typedef enum RogueVfxLayer
{
    ROGUE_VFX_LAYER_BG = 0,
    ROGUE_VFX_LAYER_MID = 1,
    ROGUE_VFX_LAYER_FG = 2,
    ROGUE_VFX_LAYER_UI = 3
} RogueVfxLayer;

/* Register or update a VFX definition with layer, lifetime (ms), and space (1=world,0=screen). */
int rogue_vfx_registry_register(const char* id, RogueVfxLayer layer, uint32_t lifetime_ms,
                                int world_space);
int rogue_vfx_registry_get(const char* id, RogueVfxLayer* out_layer, uint32_t* out_lifetime_ms,
                           int* out_world_space);
void rogue_vfx_registry_clear(void);

/* Advance active VFX instances; supports time scaling and freeze. */
void rogue_vfx_update(uint32_t dt_ms);
void rogue_vfx_set_timescale(float s);
void rogue_vfx_set_frozen(int frozen);

/* Particle emitter configuration for a registered VFX id. Returns 0 on success. */
int rogue_vfx_registry_set_emitter(const char* id, float spawn_rate_hz,
                                   uint32_t particle_lifetime_ms, int max_particles);

/* ---- VFX composition (Phase 4.3) ---- */
typedef enum RogueVfxCompMode
{
    ROGUE_VFX_COMP_NONE = 0,
    ROGUE_VFX_COMP_CHAIN = 1,   /* delays are relative to previous child spawn */
    ROGUE_VFX_COMP_PARALLEL = 2 /* delays are relative to composite start */
} RogueVfxCompMode;

/* Define a composite VFX in the registry. The composite uses its own layer/world_space/lifetime
   like a regular VFX but does not need an emitter; instead it schedules spawning of child VFX ids
   at the provided delays. Returns 0 on success.
   - chain_mode: non-zero => CHAIN, zero => PARALLEL
   - child_ids/delays_ms: arrays of length child_count; delays may be NULL (treated as 0). */
int rogue_vfx_registry_define_composite(const char* id, RogueVfxLayer layer, uint32_t lifetime_ms,
                                        int world_space, const char** child_ids,
                                        const uint32_t* delays_ms, int child_count, int chain_mode);

/* Debug/testing helpers */
int rogue_vfx_active_count(void);
int rogue_vfx_layer_active_count(RogueVfxLayer layer);
void rogue_vfx_clear_active(void);
int rogue_vfx_debug_peek_first(const char* id, int* out_world_space, float* out_x, float* out_y);

/* Spawn a VFX by id at (x,y) using the registry; returns 0 on success. */
int rogue_vfx_spawn_by_id(const char* id, float x, float y);

/* Particle system debug/query helpers */
int rogue_vfx_particles_active_count(void);
int rogue_vfx_particles_layer_count(RogueVfxLayer layer);

/* Collect active particle layers in canonical render order (BG->MID->FG->UI).
    Writes up to 'max' entries into out_layers (values are RogueVfxLayer as uint8_t).
    Returns the number of entries written. Intended for renderer iteration tests/tools. */
int rogue_vfx_particles_collect_ordered(uint8_t* out_layers, int max);

/* ---- Screen-space transform helpers (Phase 3.6) ---- */
/* Set camera for transforming world-space particles to screen coordinates.
    cam_x, cam_y are world-space camera origin; pixels_per_world is scale factor. */
void rogue_vfx_set_camera(float cam_x, float cam_y, float pixels_per_world);

/* Collect all active particles transformed into screen space.
    - out_xy: array of size (max*2) receiving x,y pairs per particle in screen space.
    - out_layers: optional parallel array of layers (RogueVfxLayer) per particle; may be NULL.
    Returns number of particles written, up to max. */
int rogue_vfx_particles_collect_screen(float* out_xy, uint8_t* out_layers, int max);

#endif /* ROGUE_EFFECTS_H */
