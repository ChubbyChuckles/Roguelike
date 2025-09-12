/* Verifies that enabling the advisory stage in no-op mode (honor=0) does not
   alter candidate counts or ordering, ensuring determinism and behavior parity. */
#include "game/collision_pipeline.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void fill_candidates(RogueCollisionCandidate* c, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i)
    {
        c[i].id = i + 100;
        c[i].x = (float) (i * 3);
        c[i].y = (float) (i * 5);
        c[i].vx = 0.0f;
        c[i].vy = 0.0f;
        c[i].half_w = c[i].half_h = 1.0f;
        c[i].layer_mask = 0xFFFFFFFFu;
        c[i].pixel_mask = NULL;
        c[i].enemy_profile = NULL;
    }
}

#define N 16u

int main(void)
{
    RogueCollisionPipeline p = {0};
    rogue_collision_pipeline_init(&p, ROGUE_COLLISION_BALANCED, 5.0f, false);
    /* Register only the advisory stage to isolate its effects. */
    assert(rogue_collision_pipeline_add_stage(&p, "advisory",
                                              rogue_collision_stage_temporal_advisory, 0.0f, 0));

    RogueCollisionCandidate base[N];
    fill_candidates(base, N);

    /* Run with advisory disabled */
    RogueCollisionCandidate a1[N];
    memcpy(a1, base, sizeof(base));
    RogueCollisionContext ctx1 = {0};
    ctx1.candidates = a1;
    ctx1.candidate_count = N;
    ctx1.advisory_enabled = 0; /* disabled */
    float sim_cost[ROGUE_COLLISION_MAX_STAGES] = {0};
    assert(rogue_collision_pipeline_execute(&p, &ctx1, sim_cost));

    /* Run with advisory enabled but honor mode off: should be identical */
    RogueCollisionCandidate a2[N];
    memcpy(a2, base, sizeof(base));
    RogueCollisionContext ctx2 = {0};
    ctx2.candidates = a2;
    ctx2.candidate_count = N;
    ctx2.advisory_enabled = 1;
    ctx2.advisory_primary_id = 1u;
    ctx2.advisory_primary_x = 0.f;
    ctx2.advisory_primary_y = 0.f;
    ctx2.advisory_primary_vx = 0.f;
    ctx2.advisory_primary_vy = 0.f;
    rogue_collision_advisory_set_honor_mode(0); /* no-op */
    assert(rogue_collision_pipeline_execute(&p, &ctx2, sim_cost));

    assert(ctx1.candidate_count == ctx2.candidate_count);
    for (uint32_t i = 0; i < N; ++i)
    {
        assert(a1[i].id == a2[i].id);
    }

    printf("temporal_advisory_parity: PASS\n");
    return 0;
}
