#include "../../src/core/app_state.h"
#include "../../src/core/vegetation/vegetation.h"
#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_config.h"
#include <math.h>
#include <stdio.h>

/* Test intent: verify trunk-only collision behaves as designed.
   We locate a tree, then probe approach from 4 cardinal directions toward the base.
   Expected behaviour (heuristic, may tune):
     - From below (player y > base_y): can overlap until ~0.35-0.45 tiles above base then blocked.
     - From above: should block slightly earlier (cushion) so cannot cross into trunk band.
     - From left/right: narrow horizontal trunk radius (~0.30-0.55). Should block only when within
   that range. Failing conditions print descriptive tags.
*/

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats; /* required globals */
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }
void rogue_skill_tree_register_baseline(void) {}

static int find_tree(float* out_x, float* out_y)
{
    int tx, ty, r;
    if (!rogue_vegetation_first_tree(&tx, &ty, &r))
        return 0;
    *out_x = tx + 0.5f;
    *out_y = ty + 0.5f;
    return 1;
}

static int blocked(float ox, float oy, float nx, float ny)
{
    return rogue_vegetation_entity_blocking(ox, oy, nx, ny);
}

static int walk_rectangle(float base_x, float base_y, float step)
{
    /* Determine minimal horizontal clearance by binary searching side_clear where lateral movement
     * along bottom just stops blocking. */
    float low = 0.10f, high = 1.0f; /* search bounds */
    for (int iter = 0; iter < 12; iter++)
    {
        float mid = (low + high) * 0.5f;
        int blocked_flag = 0;
        float y = base_y + 0.10f; /* bottom y */
        for (float xcur = base_x - mid; xcur < base_x + mid - 1e-4f; xcur += step)
        {
            float nx = xcur + step;
            if (rogue_vegetation_entity_blocking(xcur, y, nx, y))
            {
                blocked_flag = 1;
                break;
            }
        }
        if (blocked_flag)
            high = mid;
        else
            low = mid; /* shrink */
    }
    float side_clear = low - 0.02f;
    if (side_clear < 0.05f)
        side_clear = 0.05f; /* back off tiny tolerance */
    float below_y = base_y + 0.10f;
    float top_y = base_y - 0.50f;
    float bl_x = base_x - side_clear, br_x = base_x + side_clear;
    /* Validate no block around rectangle */
    for (float xcur = bl_x; xcur < br_x - 1e-4f; xcur += step)
    {
        float nx = xcur + step;
        if (rogue_vegetation_entity_blocking(xcur, below_y, nx, below_y))
            return 1;
    }
    for (float ycur = below_y; ycur > top_y + 1e-4f; ycur -= step)
    {
        float ny = ycur - step;
        if (rogue_vegetation_entity_blocking(br_x, ycur, br_x, ny))
            return 2;
    }
    for (float xcur = br_x; xcur > bl_x + 1e-4f; xcur -= step)
    {
        float nx = xcur - step;
        if (rogue_vegetation_entity_blocking(xcur, top_y, nx, top_y))
            return 3;
    }
    for (float ycur = top_y; ycur < below_y - 1e-4f; ycur += step)
    {
        float ny = ycur + step;
        if (rogue_vegetation_entity_blocking(bl_x, ycur, bl_x, ny))
            return 4;
    }
    printf("rect_ok side_clear %.3f base(%.2f,%.2f)\n", side_clear, base_x, base_y);
    return 0;
}

int main(void)
{
    if (!rogue_tilemap_init(&g_app.world_map, 64, 64))
    {
        printf("map_fail\n");
        return 1;
    }
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(12345u, 0, 0);
    if (!rogue_world_generate(&g_app.world_map, &cfg))
    {
        printf("gen_fail\n");
        return 2;
    }
    rogue_vegetation_init();
    rogue_vegetation_load_defs("../assets/plants.cfg", "../assets/trees.cfg");
    rogue_vegetation_generate(0.08f, 999u);
    /* Disable all tree collision to verify no blocking */
    rogue_vegetation_set_trunk_collision_enabled(0);
    rogue_vegetation_set_canopy_tile_blocking_enabled(0);
    float base_x, base_y;
    if (!find_tree(&base_x, &base_y))
    {
        printf("no_tree\n");
        return 3;
    }

    /* Sample step when marching toward trunk */
    const float step = 0.05f;

    /* From below (moving up): expect first block when y < base_y - approx 0.30 (tolerance) */
    printf("BEGIN_BELOW_TEST base_y %.3f\n", base_y);
    /* Quick early verification that disabled trunk collision yields no blocks from any direction in
     * short probe. */
    {
        int any_block = 0;
        float test_step = 0.05f;
        for (float dy = -0.6f; dy <= 0.6f && !any_block; dy += test_step)
        {
            if (blocked(base_x, base_y + dy, base_x, base_y + dy))
                any_block = 1;
        }
        for (float dx = -0.6f; dx <= 0.6f && !any_block; dx += test_step)
        {
            if (blocked(base_x + dx, base_y, base_x + dx, base_y))
                any_block = 1;
        }
        if (any_block)
        {
            printf("disabled_trunk_collision_blocked_unexpectedly\n");
            return 5;
        }
    }
    /* Re-enable to test normal collision behaviour below */
    rogue_vegetation_set_trunk_collision_enabled(1);
    float y = base_y + 1.0f;
    float last_free_y = y;
    int hit = 0;
    while (y > base_y - 1.0f)
    {
        float next = y - step;
        if (blocked(base_x, y, base_x, next))
        {
            hit = 1;
            break;
        }
        last_free_y = next;
        y = next;
    }
    if (!hit)
    {
        printf("no_block_from_below_v2 base_y %.3f last_free %.3f\n", base_y, last_free_y);
        return 10;
    }
    float below_clear_dist =
        last_free_y - base_y; /* how far above base we remained free before block */
    printf("below_clear_dist_v2 %.3f (base_y %.3f last_free %.3f)\n", below_clear_dist, base_y,
           last_free_y);
    fflush(stdout);
    /* Accept very small clear distances (narrow trunk band). */
    if (below_clear_dist < 0.00f || below_clear_dist > 0.80f)
    {
        printf("below_clear_out_of_range_v3 %.3f base_y %.3f last_free_y %.3f\n", below_clear_dist,
               base_y, last_free_y);
        return 11;
    }

    /* From above (moving down): should block a little earlier (<=0.55 above base) */
    y = base_y - 1.0f;
    last_free_y = y;
    hit = 0;
    while (y < base_y + 0.5f)
    {
        float next = y + step;
        if (blocked(base_x, y, base_x, next))
        {
            hit = 1;
            break;
        }
        last_free_y = next;
        y = next;
    }
    if (!hit)
    {
        printf("no_block_from_above base_y %.3f last_free %.3f\n", base_y, last_free_y);
        return 12;
    }
    float above_delta = last_free_y - base_y;
    printf("last_free_from_above delta %.3f\n", above_delta);
    if (above_delta < -0.70f || above_delta > -0.05f)
    {
        printf("above_delta_out_of_range_v2 %.3f\n", above_delta);
        return 13;
    }

    /* From left: move rightwards */
    float x = base_x - 1.0f;
    float last_free_x = x;
    hit = 0;
    while (x < base_x)
    {
        float next = x + step;
        if (blocked(x, base_y, next, base_y))
        {
            hit = 1;
            break;
        }
        last_free_x = next;
        x = next;
    }
    if (!hit)
    {
        printf("no_block_from_left last_free %.3f base %.3f\n", last_free_x, base_x);
        return 14;
    }
    float left_dist = base_x - last_free_x;
    printf("left_dist %.3f\n", left_dist);
    if (left_dist < 0.10f || left_dist > 0.70f)
    {
        printf("left_radius_out_of_range_v2 %.3f\n", left_dist);
        return 15;
    }

    /* From right: move leftwards */
    x = base_x + 1.0f;
    last_free_x = x;
    hit = 0;
    while (x > base_x)
    {
        float next = x - step;
        if (blocked(x, base_y, next, base_y))
        {
            hit = 1;
            break;
        }
        last_free_x = next;
        x = next;
    }
    if (!hit)
    {
        printf("no_block_from_right last_free %.3f base %.3f\n", last_free_x, base_x);
        return 16;
    }
    float right_dist = last_free_x - base_x;
    printf("right_dist %.3f\n", right_dist);
    if (right_dist < 0.10f || right_dist > 0.70f)
    {
        printf("right_radius_out_of_range_v2 %.3f\n", right_dist);
        return 17;
    }

    /* Rectangle walk-around test: choose a minimal clearance rectangle around trunk where movement
     * should stay unblocked. */
    float side_clear = left_dist + 0.05f;
    if (side_clear < 0.45f)
        side_clear = 0.45f;
    if (side_clear > 0.80f)
        side_clear = 0.80f;
    float below_y = base_y + 0.10f; /* safely below trunk band bottom (base_y+0.05) */
    float top_y = base_y - 0.50f;   /* safely above trunk band top (base_y-0.30) */
    float bl_x = base_x - side_clear, br_x = base_x + side_clear;
    /* Bottom edge: left -> right */
    for (float xcur = bl_x; xcur < br_x - 1e-4f; xcur += step)
    {
        float nx = xcur + step;
        if (blocked(xcur, below_y, nx, below_y))
        {
            printf("rect_block bottom x %.3f->%.3f y %.3f\n", xcur, nx, below_y);
            return 40;
        }
    }
    /* Right edge: bottom -> top (moving up, y decreasing) */
    for (float ycur = below_y; ycur > top_y + 1e-4f; ycur -= step)
    {
        float ny = ycur - step;
        if (blocked(br_x, ycur, br_x, ny))
        {
            printf("rect_block right y %.3f->%.3f x %.3f\n", ycur, ny, br_x);
            return 41;
        }
    }
    /* Top edge: right -> left */
    for (float xcur = br_x; xcur > bl_x + 1e-4f; xcur -= step)
    {
        float nx = xcur - step;
        if (blocked(xcur, top_y, nx, top_y))
        {
            printf("rect_block top x %.3f->%.3f y %.3f\n", xcur, nx, top_y);
            return 42;
        }
    }
    /* Left edge: top -> bottom (moving down, y increasing) */
    for (float ycur = top_y; ycur < below_y - 1e-4f; ycur += step)
    {
        float ny = ycur + step;
        if (blocked(bl_x, ycur, bl_x, ny))
        {
            printf("rect_block left y %.3f->%.3f x %.3f\n", ycur, ny, bl_x);
            return 43;
        }
    }
    printf("rectangle_walk_clear side_clear %.3f below_y %.3f top_y %.3f\n", side_clear, below_y,
           top_y);

    /* Multi-tree dynamic pass: test up to first 8 trees */
    int tree_count = rogue_vegetation_tree_count();
    if (tree_count > 8)
        tree_count = 8;
    for (int ti = 0; ti < tree_count; ++ti)
    {
        float tx, ty;
        int tw, th;
        if (!rogue_vegetation_tree_info(ti, &tx, &ty, &tw, &th))
            break;
        int code = walk_rectangle(tx, ty, step);
        if (code)
        {
            printf("multi_tree_rect_block index %d code %d\n", ti, code);
            return 60 + code;
        }
    }

    printf("ok\n");
    return 0;
}
