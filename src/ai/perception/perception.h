#ifndef ROGUE_AI_PERCEPTION_H
#define ROGUE_AI_PERCEPTION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Phase 3 Perception System
       Provides:
       - Line of Sight raycast (tile blocking predicate overridable for tests)
       - Vision cone + distance check with falloff
       - Hearing event ring buffer (attack / footstep) with position+loudness radius
       - Threat accumulation & decay per agent
       - Group alert broadcast (elevates nearby idle agents)
    */

    typedef enum RoguePerceptionSoundType
    {
        ROGUE_PERCEPTION_SOUND_ATTACK = 1,
        ROGUE_PERCEPTION_SOUND_FOOTSTEP = 2
    } RoguePerceptionSoundType;

    typedef struct RoguePerceptionEvent
    {
        RoguePerceptionSoundType type;
        float x, y;     /* world position */
        float loudness; /* radius within which agents can hear */
    } RoguePerceptionEvent;

#define ROGUE_PERCEPTION_EVENT_CAP 32

    typedef struct RoguePerceptionEventBuffer
    {
        RoguePerceptionEvent events[ROGUE_PERCEPTION_EVENT_CAP];
        uint8_t count;
    } RoguePerceptionEventBuffer;

    typedef struct RoguePerceptionAgent
    {
        float x, y;                     /* current world position (center) */
        float facing_x, facing_y;       /* normalized facing vector */
        float threat;                   /* accumulated threat score */
        float last_seen_x, last_seen_y; /* last seen player pos */
        float last_seen_ttl;            /* seconds until last seen pos expires */
        uint8_t has_last_seen;          /* flag */
        uint8_t alerted;                /* whether broadcast already triggered */
    } RoguePerceptionAgent;

    void rogue_perception_events_reset(void);
    /* Emit a sound event */
    void rogue_perception_emit_sound(RoguePerceptionSoundType type, float x, float y,
                                     float loudness);
    /* Access event buffer (read-only) */
    const RoguePerceptionEventBuffer* rogue_perception_events_get(void);

    /* Override blocking predicate (tx,ty tile coordinates). Pass NULL to restore default using
     * rogue_nav_is_blocked. */
    void rogue_perception_set_blocking_fn(int (*fn)(int, int));

    /* Line of sight between (ax,ay) and (bx,by) using tile stepping. Returns 1 if clear. */
    int rogue_perception_los(float ax, float ay, float bx, float by);

    /* Vision cone test. Returns 1 if target inside FOV angle (deg) & within max_dist AND LOS clear.
     * Optionally outputs distance. */
    int rogue_perception_can_see(const RoguePerceptionAgent* a, float target_x, float target_y,
                                 float fov_deg, float max_dist, float* out_dist);

    /* Tick one agent perception against target (player) state. Updates threat, last seen position &
     * TTL. */
    void rogue_perception_tick_agent(RoguePerceptionAgent* a, float dt, float player_x,
                                     float player_y, float fov_deg, float max_dist,
                                     float visible_threat_per_sec, float decay_per_sec,
                                     float last_seen_memory_sec);

    /* Process hearing events for one agent (called each frame after emission, before decay).
     * Returns number of events that contributed. */
    int rogue_perception_process_hearing(RoguePerceptionAgent* a, float player_x, float player_y,
                                         float hearing_threat, float last_seen_memory_sec);

    /* Broadcast alert from source agent to others within radius. Raises their threat to at least
     * baseline_threat and seeds last seen pos. */
    void rogue_perception_broadcast_alert(RoguePerceptionAgent* agents, int agent_count,
                                          int source_index, float radius, float baseline_threat,
                                          float last_seen_memory_sec);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_AI_PERCEPTION_H */
