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

/* -------- Phase 6: Advanced Audio (music state machine, cross-fade, ducking) -------- */

typedef enum RogueMusicState
{
    ROGUE_MUSIC_STATE_EXPLORE = 0,
    ROGUE_MUSIC_STATE_COMBAT = 1,
    ROGUE_MUSIC_STATE_BOSS = 2,
    ROGUE_MUSIC_STATE_COUNT
} RogueMusicState;

/* Register (or replace) the music track id associated with a logical music state. The track id
    must already be registered in the audio registry with category MUSIC. Returns 0 on success. */
int rogue_audio_music_register(RogueMusicState state, const char* track_id);

/* Transition to a new music state. A linear cross-fade of duration crossfade_ms is scheduled.
    If crossfade_ms == 0 the switch is immediate. Returns 0 on success. */
int rogue_audio_music_set_state(RogueMusicState state, uint32_t crossfade_ms);

/* Set or update the global music tempo (beats per minute) and beats per bar used for
    beat-aligned transitions. bpm clamped to a sane range (20..300), beats_per_bar to (1..16).
    Calling does NOT reset bar phase (time already accumulated continues). */
void rogue_audio_music_set_tempo(float bpm, int beats_per_bar);

/* Schedule a transition to the new state beginning exactly at the next bar boundary (based on
    current tempo) instead of immediately. If a cross-fade is already active or a pending
    bar-aligned transition exists, this replaces the pending one but does not interrupt an active
    fade. Returns 0 on success, <0 on error (invalid state or no track registered). */
int rogue_audio_music_set_state_on_next_bar(RogueMusicState state, uint32_t crossfade_ms);

/* Advance music system envelopes (cross-fade + ducking). Should be called each frame with the
    frame delta in milliseconds. */
void rogue_audio_music_update(uint32_t dt_ms);

/* Retrieve the primary (currently active) music track id or NULL if none. */
const char* rogue_audio_music_current(void);

/* Side-chain duck the music category: smoothly reduce music gain to target_gain (0..1) over
    attack_ms, hold for hold_ms, then restore to 1.0 over release_ms. Subsequent calls replace the
    existing envelope. All timing parameters may be zero. target_gain is clamped to [0,1]. */
void rogue_audio_duck_music(float target_gain, uint32_t attack_ms, uint32_t hold_ms,
                            uint32_t release_ms);

/* For testing / tools: return the current weight (0..1) applied to the given music track id from
    cross-fade logic (excludes duck envelope). Non-music or inactive ids return 0. */
float rogue_audio_music_track_weight(const char* track_id);

/* -------- Phase 6.4: Procedural music layering (base + random sweetener) -------- */
/* Register a sweetener (additional music track id) for a given music state with a relative gain
    (0..1) applied multiplicatively to the base state's cross-fade weight. Up to a small fixed
    number of sweeteners per state. Returns 0 on success, <0 on error (invalid state, no room,
    id missing or not MUSIC category). */
int rogue_audio_music_layer_add(RogueMusicState state, const char* sweetener_track_id, float gain);
/* Access current active sweetener track id (chosen when a state transition begins) or NULL. */
const char* rogue_audio_music_layer_current(void);
/* Retrieve configured sweetener count for a state (tests & tools). */
int rogue_audio_music_layer_count(RogueMusicState state);

/* -------- Phase 6.5: Reverb / environmental preset stubs -------- */
typedef enum RogueAudioReverbPreset
{
    ROGUE_AUDIO_REVERB_NONE = 0,
    ROGUE_AUDIO_REVERB_CAVE = 1,
    ROGUE_AUDIO_REVERB_HALL = 2,
    ROGUE_AUDIO_REVERB_CHAMBER = 3
} RogueAudioReverbPreset;
void rogue_audio_env_set_reverb_preset(RogueAudioReverbPreset preset);
RogueAudioReverbPreset rogue_audio_env_get_reverb_preset(void);
float rogue_audio_env_get_reverb_wet(void); /* current wet mix [0..1] */

/* -------- Phase 6.6: Distance-based low-pass attenuation -------- */
void rogue_audio_enable_distance_lowpass(int enable); /* non-zero = enable */
void rogue_audio_set_lowpass_params(float strength, float min_factor);
int rogue_audio_get_distance_lowpass_enabled(void);
void rogue_audio_get_lowpass_params(float* out_strength, float* out_min_factor);

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

/* -------- Phase 7.2: Blend modes (registration only; renderer consumes later) -------- */
typedef enum RogueVfxBlend
{
    ROGUE_VFX_BLEND_ALPHA = 0,
    ROGUE_VFX_BLEND_ADD = 1,
    ROGUE_VFX_BLEND_MULTIPLY = 2
} RogueVfxBlend;
int rogue_vfx_registry_set_blend(const char* id, RogueVfxBlend blend);
int rogue_vfx_registry_get_blend(const char* id, RogueVfxBlend* out_blend);

/* -------- Phase 7.4: Screen shake manager -------- */
int rogue_vfx_shake_add(float amplitude, float frequency_hz,
                        uint32_t duration_ms); /* returns handle >=0 */
void rogue_vfx_shake_clear(void);
void rogue_vfx_shake_get_offset(float* out_x, float* out_y); /* composite camera offset */

/* -------- Phase 7.6: Performance scaling (global particle emission multiplier) -------- */
void rogue_vfx_set_perf_scale(float s); /* 0..1 */
float rogue_vfx_get_perf_scale(void);

/* -------- Phase 7.1: GPU batch stub -------- */
void rogue_vfx_set_gpu_batch_enabled(int enable);
int rogue_vfx_get_gpu_batch_enabled(void);

/* ---- Parameter overrides (Phase 4.4) ---- */
typedef struct RogueVfxOverrides
{
    /* When >0, overrides the instance lifetime in ms (default: registry lifetime). */
    uint32_t lifetime_ms;
    /* When >0, scale applied to particles/sprites spawned by this instance (default 1.0). */
    float scale;
    /* ARGB color tint applied to particles; 0 means use default (0xFFFFFFFF). */
    uint32_t color_rgba;
} RogueVfxOverrides;

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
/* Spawn with optional overrides (lifetime/scale/color). Pass NULL for defaults. */
int rogue_vfx_spawn_with_overrides(const char* id, float x, float y, const RogueVfxOverrides* ov);

/* Particle system debug/query helpers */
int rogue_vfx_particles_active_count(void);
int rogue_vfx_particles_layer_count(RogueVfxLayer layer);

/* Collect active particle layers in canonical render order (BG->MID->FG->UI).
    Writes up to 'max' entries into out_layers (values are RogueVfxLayer as uint8_t).
    Returns the number of entries written. Intended for renderer iteration tests/tools. */
int rogue_vfx_particles_collect_ordered(uint8_t* out_layers, int max);

/* Debug helpers to collect particle attributes for tests/tools. Return count written up to max. */
int rogue_vfx_particles_collect_scales(float* out_scales, int max);
int rogue_vfx_particles_collect_colors(uint32_t* out_rgba, int max);
/* Phase 4.5: Collect particle lifetimes (ms). */
int rogue_vfx_particles_collect_lifetimes(uint32_t* out_ms, int max);

/* ---- Gameplay -> Effects mapping (Phase 5.1) ---- */
typedef enum RogueFxMapType
{
    ROGUE_FX_MAP_AUDIO = 1,
    ROGUE_FX_MAP_VFX = 2
} RogueFxMapType;

/* Register a mapping from a gameplay event key to an effect id of the given map type.
    Multiple entries may be registered for the same key and will all be emitted when triggered.
    The provided priority is applied to the emitted FX bus event. Returns 0 on success. */
int rogue_fx_map_register(const char* gameplay_event_key, RogueFxMapType type,
                          const char* effect_id, RogueEffectPriority priority);

/* Clear all gameplay->effects mappings. */
void rogue_fx_map_clear(void);

/* Trigger a gameplay event by key; enqueues corresponding FX events (audio/vfx) on the FX bus.
    For VFX entries, (x,y) are used as the spawn position. For audio entries, (x,y) feed positional
    attenuation when enabled. Returns number of FX events enqueued. */
int rogue_fx_trigger_event(const char* gameplay_event_key, float x, float y);

/* ---- Damage event hook (Phase 5.2) ---- */
/* Bind an observer to combat damage events that translates events into gameplay keys and triggers
     mapped FX automatically. Key scheme:
         - "damage/<type>/hit" always fires for any applied hit of that type
         - If ev->crit==1, also fires "damage/<type>/crit"
         - If ev->execution==1, also fires "damage/<type>/execution"
     Returns 1 on success (or already bound), 0 on failure. */
int rogue_fx_damage_hook_bind(void);
/* Unbind the damage event observer if bound. */
void rogue_fx_damage_hook_unbind(void);

/* ---- Random distributions (Phase 4.5) ---- */
typedef enum RogueVfxDist
{
    ROGUE_VFX_DIST_NONE = 0,
    ROGUE_VFX_DIST_UNIFORM = 1, /* a=min, b=max (inclusive bounds for multiplier) */
    ROGUE_VFX_DIST_NORMAL = 2   /* a=mean, b=sigma (multiplier; values <=0 clamped) */
} RogueVfxDist;

/* Configure per-VFX particle variation distributions for scale and lifetime.
   For UNIFORM: a=min, b=max multipliers applied to base value (scale default 1.0; lifetime in ms).
   For NORMAL: a=mean, b=sigma multipliers; values <= 0 are clamped to a small positive epsilon.
   Pass mode=ROGUE_VFX_DIST_NONE to disable that variation channel.
   Returns 0 on success. */
int rogue_vfx_registry_set_variation(const char* id, RogueVfxDist scale_mode, float scale_a,
                                     float scale_b, RogueVfxDist lifetime_mode, float life_a,
                                     float life_b);

/* Testing/debug: set deterministic FX RNG seed (affects audio variation and VFX distributions). */
void rogue_fx_debug_set_seed(uint32_t seed);

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
