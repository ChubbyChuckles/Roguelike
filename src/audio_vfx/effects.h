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

#endif /* ROGUE_EFFECTS_H */
