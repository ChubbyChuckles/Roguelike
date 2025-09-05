#include "world_gen.h"
#include <stdio.h>
#include <string.h>

static int sum4(const int a[4]) { return a[0] + a[1] + a[2] + a[3]; }

int main(void)
{
    int c0[4];
    int c1[4];

    /* Representative shallow depth and a mid depth */
    int rc = rogue_dungeon_debug_sample_reward_tier(/*depth*/ 2, /*bump*/ 0, /*reps*/ 5000, c0);
    if (rc != 0)
    {
        fprintf(stderr, "sampler failed (0)\n");
        return 1;
    }
    if (sum4(c0) != 5000)
    {
        fprintf(stderr, "sum mismatch (0) %d\n", sum4(c0));
        return 1;
    }
    /* Bounds: only 0..3 should be filled, implicitly ensured by API */
    for (int i = 0; i < 4; ++i)
        if (c0[i] < 0)
        {
            fprintf(stderr, "neg count\n");
            return 1;
        }

    rc = rogue_dungeon_debug_sample_reward_tier(/*depth*/ 6, /*bump*/ 0, /*reps*/ 5000, c1);
    if (rc != 0)
    {
        fprintf(stderr, "sampler failed (1)\n");
        return 1;
    }
    if (sum4(c1) != 5000)
    {
        fprintf(stderr, "sum mismatch (1) %d\n", sum4(c1));
        return 1;
    }

    /* Sanity: higher depth shifts mass towards higher tiers (coarse) */
    int low0 = c0[0] + c0[1];
    int low1 = c1[0] + c1[1];
    int high0 = c0[2] + c0[3];
    int high1 = c1[2] + c1[3];
    if (!(high1 > high0 || low1 < low0))
    {
        fprintf(stderr, "expected depth to increase high-tier mass\n");
        return 1;
    }

    /* Bump behavior at fixed depth: bump_count>0 should increase high-tier mass. */
    int b0[4], b1[4];
    rc = rogue_dungeon_debug_sample_reward_tier(/*depth*/ 6, /*bump*/ 0, /*reps*/ 5000, b0);
    if (rc != 0)
    {
        fprintf(stderr, "sampler failed (b0)\n");
        return 1;
    }
    rc = rogue_dungeon_debug_sample_reward_tier(/*depth*/ 6, /*bump*/ 2, /*reps*/ 5000, b1);
    if (rc != 0)
    {
        fprintf(stderr, "sampler failed (b1)\n");
        return 1;
    }
    int b0_high = b0[2] + b0[3];
    int b1_high = b1[2] + b1[3];
    if (!(b1_high > b0_high))
    {
        fprintf(stderr, "expected bump to increase high-tier mass (%d -> %d)\n", b0_high, b1_high);
        return 1;
    }

    /* Edge bounds: extreme depths still clamp to 0..3 tiers and produce counts. */
    int e0[4], e1[4];
    rc = rogue_dungeon_debug_sample_reward_tier(/*depth*/ -100, /*bump*/ 2, /*reps*/ 1000, e0);
    if (rc != 0)
    {
        fprintf(stderr, "sampler failed (e0)\n");
        return 1;
    }
    rc = rogue_dungeon_debug_sample_reward_tier(/*depth*/ 1000, /*bump*/ 2, /*reps*/ 1000, e1);
    if (rc != 0)
    {
        fprintf(stderr, "sampler failed (e1)\n");
        return 1;
    }
    if (sum4(e0) != 1000 || sum4(e1) != 1000)
    {
        fprintf(stderr, "edge sums mismatch\n");
        return 1;
    }

    return 0;
}
