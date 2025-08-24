#ifndef ROGUE_UI_ANIMATION_H
#define ROGUE_UI_ANIMATION_H

#include "ui_context.h" /* for RogueUIEaseType */
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Interrupt policy */
    /* APPEND currently behaves like REPLACE (future extension: queue). Documented accordingly. */
    typedef enum RogueUITimelinePolicy
    {
        ROGUE_UI_TIMELINE_REPLACE = 0,
        ROGUE_UI_TIMELINE_IGNORE = 1,
        ROGUE_UI_TIMELINE_APPEND = 2
    } RogueUITimelinePolicy;

    typedef struct RogueUITimelineKeyframe
    {
        float at;             /* 0..1 normalized position */
        float scale;          /* multiplicative scale (1 = identity) */
        float alpha;          /* 0..1 */
        RogueUIEaseType ease; /* easing applied for segment leading to this keyframe */
    } RogueUITimelineKeyframe;

    void rogue_ui_timeline_play(struct RogueUIContext* ctx, uint32_t id_hash,
                                const RogueUITimelineKeyframe* kfs, int count, float duration_ms,
                                RogueUITimelinePolicy policy);

    void rogue_ui_timeline_step(struct RogueUIContext* ctx, double dt_ms);
    float rogue_ui_timeline_scale(const struct RogueUIContext* ctx, uint32_t id_hash,
                                  int* active_out);
    float rogue_ui_timeline_alpha(const struct RogueUIContext* ctx, uint32_t id_hash,
                                  int* active_out);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_UI_ANIMATION_H */
