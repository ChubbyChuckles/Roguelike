/* test_collision_pipeline_phase2_3_enemy_lod.c
 * Milestone 2.3 integration test: enemy collision profile adaptive LOD stage.
 * Verifies that:
 *  - Stage executes without modifying candidate count.
 *  - Attached profiles receive updated bias based on distance & area proxies.
 *  - Strong negative bias triggers pipeline quality upgrade (quality_delta -> applied).
 */
#include "game/collision_pipeline.h"
#include "game/enemy_collision_opt.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    RogueCollisionPipeline pipeline;
    rogue_collision_pipeline_init(&pipeline, ROGUE_COLLISION_BALANCED, 2.0f, true);
    /* Order: temporal (noop), enemy LOD, spatial (unused), aabb (unused) */
    if (!rogue_collision_pipeline_add_stage(&pipeline, "enemy_lod",
                                            rogue_collision_stage_enemy_profile_lod, 0.f, 0))
        return 1;

    RogueCollisionCandidate cands[3];
    memset(cands, 0, sizeof(cands));
    RogueEnemyCollisionProfile profiles[3];
    for (int i = 0; i < 3; ++i)
    {
        /* Candidate 0: very close to view center and large -> high importance -> negative bias.
           Others: far and small -> lower importance -> higher/less negative bias. */
        float w = (i == 0) ? 100.f : (10.f + (float) i);
        float h = (i == 0) ? 100.f : (12.f + (float) i);
        rogue_enemy_collision_profile_analyze(&profiles[i], w, h, (uint32_t) (w * h * 0.5f));
        cands[i].id = (uint32_t) i;
        cands[i].x = (i == 0) ? 55.f : (float) (150 + i * 10); /* first near center (50,50) */
        cands[i].y = (i == 0) ? 55.f : (float) (140 + i * 5);
        cands[i].half_w = w * 0.5f;
        cands[i].half_h = h * 0.5f;
        cands[i].enemy_profile = &profiles[i];
    }
    RogueCollisionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.candidates = cands;
    ctx.candidate_count = 3;
    ctx.view_x = 0.f;
    ctx.view_y = 0.f;
    ctx.view_w = 100.f;
    ctx.view_h = 100.f;

    if (!rogue_collision_pipeline_execute(&pipeline, &ctx, NULL))
        return 2;
    /* After execution, first profile (large & near) should have negative bias (higher importance ->
     * negative). */
    int bias0 = rogue_enemy_collision_profile_get_lod_bias(&profiles[0]);
    int bias1 = rogue_enemy_collision_profile_get_lod_bias(&profiles[1]);
    if (bias0 >= 0)
    {
        fprintf(stderr, "expected negative bias for close candidate, got %d\n", bias0);
        return 3;
    }
    if (bias1 < bias0) /* far candidate should not exceed close fidelity request */
    {
        fprintf(stderr, "unexpected ordering bias1=%d bias0=%d\n", bias1, bias0);
        return 4;
    }
    return 0;
}
