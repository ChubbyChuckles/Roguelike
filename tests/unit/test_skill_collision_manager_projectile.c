/* test_skill_collision_manager_projectile.c
 * Projectile + frame interpolation scaffolding test (Milestone 3.1 deepening)
 */
#include "game/skill_collision_manager.h"
#include <stdio.h>
#include <string.h>

static int float_near(float a, float b, float eps) { return (a - b < eps && b - a < eps); }

int main(void)
{
    RogueSkillCollisionEffect effect;
    rogue_skill_collision_effect_init(&effect);

    RogueSkillCollisionLayer proj = {0};
    proj.type = ROGUE_SKILL_PROJECTILE;
    proj.start_time_ms = 0.f;
    proj.duration_ms = 200.f;
    proj.affected_layers = 0x1;
    proj.pierces_enemies = 1; /* allow multiple targets per tick */
    proj.max_targets = 0;     /* unlimited */
    proj.proj_pos_x = 0.f;
    proj.proj_pos_y = 0.f;
    proj.proj_vel_x = 0.1f; /* 0.1 units per ms => 10 units over 100ms */
    proj.proj_vel_y = 0.f;
    proj.proj_radius = 0.75f; /* small radius */
    for (int i = 0; i < 8; ++i)
        proj.intensity_curve[i] = 1.f; /* flat */
    proj.frame_count = 5;              /* frame interpolation test */

    if (rogue_skill_collision_effect_add_layer(&effect, &proj) != 0)
    {
        fprintf(stderr, "add layer failed\n");
        return 1;
    }

    RogueSkillCollisionTarget targets[4];
    /* Place targets along X axis: positions at x=2,5,9,20 */
    targets[0].id = 1;
    targets[0].layer_mask = 0x1;
    targets[0].x = 2.f;
    targets[0].y = 0.f;
    targets[1].id = 2;
    targets[1].layer_mask = 0x1;
    targets[1].x = 5.f;
    targets[1].y = 0.f;
    targets[2].id = 3;
    targets[2].layer_mask = 0x1;
    targets[2].x = 9.f;
    targets[2].y = 0.f;
    targets[3].id = 4;
    targets[3].layer_mask = 0x1;
    targets[3].x = 20.f;
    targets[3].y = 0.f;

    RogueSkillCollisionHit hits[32];
    RogueSkillCollisionHitBuffer buf = {hits, 32, 0};

    /* Tick in 50ms increments; projectile speed 0.1 units/ms => 5 units per 50ms */
    rogue_skill_collision_effect_tick(&effect, 50.f, targets, 4, &buf);
    /* Position ~5, should collide with target at x=5 only (radius 0.75) */
    if (buf.count != 1 || buf.hits[0].target_id != 2)
    {
        fprintf(stderr, "expected hit target 2 at t=50ms, got count=%u id=%u\n", buf.count,
                buf.hits[0].target_id);
        return 2;
    }
    float frame_idx1 = rogue_skill_collision_layer_frame_index(&effect.layers[0]);
    if (frame_idx1 <= 0.f)
    {
        fprintf(stderr, "expected frame idx >0 after progress\n");
        return 3;
    }

    rogue_skill_collision_effect_tick(&effect, 50.f, targets, 4, &buf);
    /* Position ~10, should now also collide with target at x=9 (new hit). target 2 may hit again
     * (piercing) */
    if (buf.count < 2)
    {
        fprintf(stderr, "expected at least 2 hits by 100ms\n");
        return 4;
    }
    int saw3 = 0;
    for (uint32_t i = 0; i < buf.count; ++i)
        if (buf.hits[i].target_id == 3)
            saw3 = 1;
    if (!saw3)
    {
        fprintf(stderr, "expected target 3 hit by 100ms\n");
        return 5;
    }

    float frame_idx2 = rogue_skill_collision_layer_frame_index(&effect.layers[0]);
    if (!(frame_idx2 > frame_idx1))
    {
        fprintf(stderr, "frame index did not advance\n");
        return 6;
    }

    /* Advance beyond duration to finish */
    rogue_skill_collision_effect_tick(&effect, 150.f, targets, 4, &buf);
    if (!rogue_skill_collision_effect_finished(&effect))
    {
        fprintf(stderr, "effect not finished\n");
        return 7;
    }

    return 0; /* success */
}
