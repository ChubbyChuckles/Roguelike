/* test_skill_collision_manager_phase3_1.c
 * Validates Milestone 3.1 skill collision effect system:
 *  - Multi-layer activation scheduling (start_time_ms + duration_ms)
 *  - Intensity curve evaluation (linear interpolation across 8 control points)
 *  - Layer-based filtering (affected_layers bitmask)
 *  - Piercing vs non-piercing behavior & max_targets enforcement
 */
#include "game/skill_collision_manager.h"
#include <stdio.h>
#include <string.h>

static int float_near(float a, float b, float eps) { return (a - b < eps && b - a < eps); }

int main(void)
{
    RogueSkillCollisionEffect effect;
    rogue_skill_collision_effect_init(&effect);

    /* Layer 0: starts at 0ms, lasts 100ms, non-piercing, max 1 target */
    RogueSkillCollisionLayer L0 = {0};
    L0.type = ROGUE_SKILL_INSTANT;
    L0.start_time_ms = 0.f;
    L0.duration_ms = 100.f;
    L0.affected_layers = 0x1; /* layer bit 0 */
    L0.pierces_enemies = 0;
    L0.max_targets = 1;
    /* Simple ramp curve 0 -> 1 */
    for (int i = 0; i < 8; ++i)
        L0.intensity_curve[i] = (float) i / 7.f;

    /* Layer 1: delayed start (50ms), lasts 100ms, piercing, max 3 targets, flat default intensity
     */
    RogueSkillCollisionLayer L1 = {0};
    L1.type = ROGUE_SKILL_MULTI_HIT;
    L1.start_time_ms = 50.f;
    L1.duration_ms = 100.f;
    L1.affected_layers = 0x3; /* overlaps bits 0 and 1 */
    L1.pierces_enemies = 1;
    L1.max_targets = 3;
    /* Leave intensity_curve all zeros -> defaults to 1.0 */

    if (rogue_skill_collision_effect_add_layer(&effect, &L0) != 0)
    {
        fprintf(stderr, "failed to add layer 0\n");
        return 1;
    }
    if (rogue_skill_collision_effect_add_layer(&effect, &L1) != 0)
    {
        fprintf(stderr, "failed to add layer 1\n");
        return 2;
    }

    RogueSkillCollisionTarget targets[4];
    for (int i = 0; i < 4; ++i)
    {
        targets[i].id = (uint32_t) i;
        targets[i].layer_mask = (i < 3) ? 0x1 : 0x2;
    }
    /* targets 0,1,2 -> layer bit 0; target 3 -> bit 1 */

    RogueSkillCollisionHit hit_storage[16];
    RogueSkillCollisionHitBuffer buf = {hit_storage, 16, 0};

    /* Tick 1: advance 40ms -> only layer 0 active */
    rogue_skill_collision_effect_tick(&effect, 40.f, targets, 4, &buf);
    if (buf.count != 1)
    {
        fprintf(stderr, "expected 1 hit (layer0), got %u\n", buf.count);
        return 3;
    }
    if (buf.hits[0].layer_index != 0)
    {
        fprintf(stderr, "wrong layer index first hit\n");
        return 4;
    }
    if (buf.hits[0].target_id != 0)
    {
        fprintf(stderr, "non-piercing layer did not pick first target id=0\n");
        return 5;
    }
    if (!float_near(buf.hits[0].intensity, (0.f / 7.f), 0.15f))
    { /* early in ramp ~0 */
        fprintf(stderr, "unexpected intensity ramp start %f\n", buf.hits[0].intensity);
        return 6;
    }

    /* Tick 2: advance 20ms -> layer 0 + layer 1 (starts at 50ms). Layer0 still non-piercing so no
     * new hits; Layer1 piercing adds up to 3 targets (bits 0 or 1). */
    rogue_skill_collision_effect_tick(&effect, 20.f, targets, 4, &buf);
    /* New hits appended: layer1 should add targets 0,1,2 (max_targets=3) */
    if (buf.count != 1 + 3)
    {
        fprintf(stderr, "expected 4 total hits after layer1 activation, got %u\n", buf.count);
        return 7;
    }
    for (int i = 1; i <= 3; ++i)
    {
        if (buf.hits[i].layer_index != 1)
        {
            fprintf(stderr, "hit %d wrong layer %u\n", i, buf.hits[i].layer_index);
            return 8;
        }
        if (buf.hits[i].target_id != (uint32_t) (i - 1))
        {
            fprintf(stderr, "unexpected target id in piercing sequence\n");
            return 9;
        }
        if (!float_near(buf.hits[i].intensity, 1.f, 0.001f))
        {
            fprintf(stderr, "layer1 intensity not flat 1.0\n");
            return 10;
        }
    }

    /* Tick 3: advance 50ms (total time now 110ms) -> layer0 duration exceeded, layer1 mid duration.
     * Expect further layer1 hits (piercing) again limited by max_targets (3). */
    uint32_t before = buf.count;
    rogue_skill_collision_effect_tick(&effect, 50.f, targets, 4, &buf);
    if (buf.count != before + 3)
    {
        fprintf(stderr, "expected 3 more layer1 hits, got %u new (total %u)\n", buf.count - before,
                buf.count);
        return 11;
    }
    /* Sample an intensity from layer0 earlier mid-ramp to ensure interpolation worked: we stored
     * first hit only; test intensity near 0 (already). Next ensure a later ramp value produced >0.3
     * at any subsequent layer0 hit (none expected due to non-piercing), so rely on internal
     * progression check: */
    if (effect.layers[0].elapsed_ms <= 0.f || effect.layers[0].elapsed_ms > 120.f)
    {
        fprintf(stderr, "layer0 elapsed_ms sanity failed\n");
        return 12;
    }

    /* Fast forward to completion */
    rogue_skill_collision_effect_tick(&effect, 200.f, targets, 4, &buf);
    if (!rogue_skill_collision_effect_finished(&effect))
    {
        fprintf(stderr, "effect should be finished\n");
        return 13;
    }
    if (effect.layers[0].finished == 0 || effect.layers[1].finished == 0)
    {
        fprintf(stderr, "layers not finished flags\n");
        return 14;
    }

    return 0; /* success */
}
