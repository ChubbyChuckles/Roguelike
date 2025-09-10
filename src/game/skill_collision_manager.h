/* skill_collision_manager.h - Milestone 3.1: Multi-Layer Skill Effect Collision Pipeline (initial
 * slice) Implements the complete Milestone 3.1 task list:
 *   - Multi-layered skill effect system (up to 4 layers per effect)
 *   - Time-based intensity curves (8 control points, linear interpolation)
 *   - Layer-based collision filtering (bitmask overlap)
 *   - Piercing & max target limitation mechanics
 *
 * Design notes:
 *  - This slice purposefully provides a deterministic, headless‑safe CPU implementation
 *    with no dependency on runtime rendering or existing skill systems. It is sandboxed
 *    (not wired into gameplay) to avoid regression risk.
 *  - Collision evaluation is simplified: targets are presented as an array of logical
 *    collision candidates (id + layer_mask). If a layer is active during the update step
 *    and filtering passes, the target is considered a hit for that layer until its
 *    per-layer max target cap is reached. When pierces_enemies==false only the first
 *    accepted target per layer is recorded (legacy melee semantics scaffold).
 *  - Intensity curve evaluation: 8 samples (uniformly spaced 0..1). A layer without
 *    explicit curve data (all zeros) defaults to a flat 1.0 intensity. Curves are sampled
 *    using linear interpolation between the surrounding control points.
 *  - Re-entrancy: Effect state is advanced only via rogue_skill_collision_effect_tick; the
 *    caller supplies dt_ms per frame. The effect reports completion when all layers have
 *    elapsed past their (start+duration) windows.
 *
 * Future (later milestones):
 *  - Real spatial queries (AOE shapes, projectile trajectories, mask-based overlap)
 *  - Continuous collision / multi-frame hit cooldowns
 *  - Integration with weapon / pixel-perfect systems
 *  - Variable curve keyframe counts & non-uniform timestamps
 */
#ifndef ROGUE_GAME_SKILL_COLLISION_MANAGER_H
#define ROGUE_GAME_SKILL_COLLISION_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RogueSkillCollisionType
    {
        ROGUE_SKILL_INSTANT = 0,
        ROGUE_SKILL_PROJECTILE = 1,
        ROGUE_SKILL_AOE_EXPANDING = 2,
        ROGUE_SKILL_AOE_PERSISTENT = 3,
        ROGUE_SKILL_CHANNELED = 4,
        ROGUE_SKILL_MULTI_HIT = 5
    } RogueSkillCollisionType;

    /* Logical collision target provided by caller (test scaffolding). */
    typedef struct RogueSkillCollisionTarget
    {
        uint32_t id;         /* Stable identifier */
        uint32_t layer_mask; /* Collision layers this target belongs to */
        float x;             /* Position (for projectile radius checks) */
        float y;             /* Position (for projectile radius checks) */
    } RogueSkillCollisionTarget;

    typedef struct RogueSkillCollisionLayer
    {
        RogueSkillCollisionType type;
        float start_time_ms;      /* Activation start */
        float duration_ms;        /* Active window length */
        float intensity_curve[8]; /* 8 control points 0..1 (uniform over duration) */
        uint8_t frame_count;      /* Reserved for animated masks (unused this slice) */
        uint32_t affected_layers; /* Bitmask filter */
        uint8_t pierces_enemies;  /* Bool semantics */
        uint8_t max_targets;      /* Cap (0 => unlimited) */
        /* Projectile (type == ROGUE_SKILL_PROJECTILE) runtime/config fields */
        float proj_pos_x;  /* Current projectile X (advanced each tick after activation) */
        float proj_pos_y;  /* Current projectile Y */
        float proj_vel_x;  /* Velocity X units per ms */
        float proj_vel_y;  /* Velocity Y units per ms */
        float proj_radius; /* Collision radius */
        /* Runtime state */
        float elapsed_ms;      /* Accumulated local time (relative to layer start) */
        uint8_t active;        /* Set while within active window */
        uint8_t finished;      /* Set after duration elapsed */
        uint8_t hits_recorded; /* Number of targets recorded so far */
    } RogueSkillCollisionLayer;

    typedef struct RogueSkillCollisionEffect
    {
        RogueSkillCollisionLayer layers[4];
        uint8_t layer_count;
        float total_duration_ms; /* Derived = max(layer.start+duration) */
        float global_time_ms;    /* Time since effect creation */
        uint32_t skill_id;
        uint32_t caster_entity_id;
        uint8_t requires_line_of_sight; /* Placeholder flag */
        uint8_t effect_finished;        /* All layers finished */
    } RogueSkillCollisionEffect;

    typedef struct RogueSkillCollisionHit
    {
        uint32_t target_id;
        uint8_t layer_index; /* Which layer produced this hit */
        float time_ms;       /* Global time at hit */
        float intensity;     /* Evaluated intensity at hit time */
    } RogueSkillCollisionHit;

    typedef struct RogueSkillCollisionHitBuffer
    {
        RogueSkillCollisionHit* hits;
        uint32_t capacity;
        uint32_t count;
    } RogueSkillCollisionHitBuffer;

    /* Initialize an empty effect. */
    void rogue_skill_collision_effect_init(RogueSkillCollisionEffect* e);

    /* Add a layer. Returns 0 on success, -1 on overflow. Caller provides configuration; runtime
     * state is zeroed. Any intensity_curve that is entirely zeroed will be treated as a flat 1.0.
     */
    int rogue_skill_collision_effect_add_layer(RogueSkillCollisionEffect* e,
                                               const RogueSkillCollisionLayer* src);

    /* Evaluate the normalized intensity (0..1) for a layer at its local time (elapsed_ms).
     * Applies default flat=1.0 when curve contains only zeros. */
    float rogue_skill_collision_layer_intensity(const RogueSkillCollisionLayer* l);

    /* Frame interpolation helper (scaffolding). Returns fractional frame index 0..(frame_count-1)
     * based on elapsed_ms over duration. If frame_count <= 1 returns 0.0f. */
    float rogue_skill_collision_layer_frame_index(const RogueSkillCollisionLayer* l);

    /* Simple projectile test helper: returns 1 if (target_x,target_y) lies within layer's
     * projectile radius (uses current proj_pos_* fields). Returns 0 if not a projectile layer */
    int rogue_skill_collision_test_projectile(const RogueSkillCollisionLayer* l, float target_x,
                                              float target_y);

    /* Advance the effect by dt_ms and perform collision evaluation against provided targets.
     * Writes hit records (up to buffer capacity). Returns number of hits appended this call. */
    uint32_t rogue_skill_collision_effect_tick(RogueSkillCollisionEffect* e, float dt_ms,
                                               const RogueSkillCollisionTarget* targets,
                                               uint32_t target_count,
                                               RogueSkillCollisionHitBuffer* out_hits);

    /* Convenience: true if effect has completed all layers. */
    static inline int rogue_skill_collision_effect_finished(const RogueSkillCollisionEffect* e)
    {
        return e ? (e->effect_finished != 0) : 1;
    }

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_GAME_SKILL_COLLISION_MANAGER_H */
