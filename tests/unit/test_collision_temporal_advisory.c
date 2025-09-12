#include "game/collision_pipeline.h"
#include <assert.h>
#include <stdio.h>

/* Minimal test: advisory predictor integration collects metrics without altering behavior.
   We construct a context with a primary id and a small set of candidates and run the advisory
   stage. We assert that candidate_count is unchanged and that advisory metrics increase. */

static void fill_candidate(RogueCollisionCandidate* c, uint32_t id, float x, float y, float vx,
                           float vy)
{
    c->id = id;
    c->x = x;
    c->y = y;
    c->vx = vx;
    c->vy = vy;
    c->half_w = c->half_h = 8.f;
    c->layer_mask = 0xFFFFFFFFu;
    c->pixel_mask = NULL;
    c->pixel_mask_lx = c->pixel_mask_ly = 0;
    c->enemy_profile = NULL;
}

int main(void)
{
    RogueCollisionCandidate cand[4];
    fill_candidate(&cand[0], 101, 10.f, 10.f, 0.f, 0.f);
    fill_candidate(&cand[1], 102, 18.f, 10.f, 0.1f, 0.f);
    fill_candidate(&cand[2], 103, 40.f, 10.f, -0.05f, 0.f);
    fill_candidate(&cand[3], 104, 200.f, 100.f, 0.f, 0.f);

    RogueCollisionContext ctx;
    ctx.candidates = cand;
    ctx.candidate_count = 4;
    ctx.quality_level = ROGUE_COLLISION_BALANCED;
    ctx.user_data = NULL;
    ctx.view_x = 0.f;
    ctx.view_y = 0.f;
    ctx.view_w = 128.f;
    ctx.view_h = 128.f;
    ctx.quality_delta = 0;
    ctx.skip_spatial = 0;
    ctx.advisory_enabled = 1;
    ctx.advisory_primary_id = 1u;
    ctx.advisory_primary_x = 0.f;
    ctx.advisory_primary_y = 0.f;
    ctx.advisory_primary_vx = 0.f;
    ctx.advisory_primary_vy = 0.f;

    RogueCollisionMetrics m = {0};

    /* Reset metrics and run advisory twice to see metrics grow. */
    rogue_collision_advisory_reset(12.f);
    uint32_t p0 = 0, u0 = 0;
    rogue_collision_advisory_get_metrics(&p0, &u0);
    assert(p0 == 0 && u0 == 0);

    bool cont = rogue_collision_stage_temporal_advisory(&ctx, &m);
    assert(cont);
    assert(m.output_candidates == 4);

    uint32_t p1 = 0, u1 = 0;
    rogue_collision_advisory_get_metrics(&p1, &u1);
    /* We updated predictor entries for each candidate: updates should be >= 4. */
    assert(u1 >= 4);

    /* Run again with slightly increased separation for one candidate to allow a conservative skip.
     */
    cand[0].x += 1.0f;
    cand[0].y += 0.0f; /* increases sep2 slightly */
    RogueCollisionMetrics m2 = {0};
    cont = rogue_collision_stage_temporal_advisory(&ctx, &m2);
    assert(cont);
    uint32_t p2 = 0, u2 = 0;
    rogue_collision_advisory_get_metrics(&p2, &u2);
    assert(u2 >= u1 + 4 - 1); /* at least roughly per-candidate touches */
    assert(p2 >= p1);         /* predictions monotonic */

    /* Behavior must not change: candidate count remains the same */
    assert(ctx.candidate_count == 4);
    (void) printf("advisory metrics: predicts=%u updates=%u\n", p2, u2);
    return 0;
}
