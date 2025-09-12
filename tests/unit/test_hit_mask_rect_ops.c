/* test_hit_mask_rect_ops.c
 * Verifies correctness of fast rectangle queries and SIMD/scalar parity.
 */
#include "game/hit_pixel_mask.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_checker(RogueHitPixelMaskFrame* f, int w, int h, int period)
{
    memset(f, 0, sizeof(*f));
    f->width = w;
    f->height = h;
    f->origin_x = 0;
    f->origin_y = 0;
    f->pitch_words = (w + 31) / 32;
    size_t words = (size_t) f->pitch_words * (size_t) h;
    f->bits = (uint32_t*) malloc(words * sizeof(uint32_t));
    memset(f->bits, 0, words * sizeof(uint32_t));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (((x / period) + (y / period)) % 2 == 0)
                rogue_hit_mask_set(f, x, y);
}

static void free_frame(RogueHitPixelMaskFrame* f)
{
    if (f->bits)
        free(f->bits);
    memset(f, 0, sizeof(*f));
}

static int run_correctness(void)
{
    RogueHitPixelMaskFrame f = {0};
    make_checker(&f, 64, 32, 4);
    /* Query a region we know has set bits */
    if (!rogue_hit_mask_any_set_in_rect(&f, 0, 0, 8, 8))
    {
        fprintf(stderr, "expected bits in 0,0..8x8\n");
        free_frame(&f);
        return 1;
    }
    /* Query a region outside bounds -> clipped -> still should find */
    if (!rogue_hit_mask_any_set_in_rect(&f, -8, -8, 16, 16))
    {
        fprintf(stderr, "expected bits in clipped region\n");
        free_frame(&f);
        return 2;
    }
    /* Query a zero-area or empty region */
    if (rogue_hit_mask_any_set_in_rect(&f, 10, 10, 0, 10))
    {
        fprintf(stderr, "unexpected hit for zero width\n");
        free_frame(&f);
        return 3;
    }
    /* Intersect two identical regions */
    RogueHitPixelMaskFrame g = {0};
    make_checker(&g, 64, 32, 4);
    if (!rogue_hit_mask_intersect_any_same_origin(&f, 0, 0, &g, 0, 0, 32, 16))
    {
        fprintf(stderr, "expected intersection on identical masks\n");
        free_frame(&f);
        free_frame(&g);
        return 4;
    }
    /* Intersect disjoint by offset */
    if (rogue_hit_mask_intersect_any_same_origin(&f, 0, 0, &g, 33, 0, 16, 16))
    {
        fprintf(stderr, "unexpected intersection for disjoint regions\n");
        free_frame(&f);
        free_frame(&g);
        return 5;
    }
    free_frame(&f);
    free_frame(&g);
    return 0;
}

static int run_parity(void)
{
    RogueHitPixelMaskFrame a = {0}, b = {0};
    make_checker(&a, 96, 64, 3);
    make_checker(&b, 96, 64, 5);
    int cases[][6] = {
        {0, 0, 0, 0, 64, 32},
        {5, 7, 11, 13, 33, 9},
        {-4, -4, 0, 0, 12, 12},
        {40, 10, 45, 20, 48, 16},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    /* scalar */
    rogue_hit_mask_simd_set_enabled(0);
    int scalar_any = 0;
    for (int i = 0; i < ncases; ++i)
        scalar_any |= rogue_hit_mask_intersect_any_same_origin(
            &a, cases[i][0], cases[i][1], &b, cases[i][2], cases[i][3], cases[i][4], cases[i][5]);
    /* simd */
    rogue_hit_mask_simd_set_enabled(1);
    int simd_any = 0;
    for (int i = 0; i < ncases; ++i)
        simd_any |= rogue_hit_mask_intersect_any_same_origin(
            &a, cases[i][0], cases[i][1], &b, cases[i][2], cases[i][3], cases[i][4], cases[i][5]);
    free_frame(&a);
    free_frame(&b);
    if (scalar_any != simd_any)
    {
        fprintf(stderr, "parity mismatch scalar=%d simd=%d\n", scalar_any, simd_any);
        return 10;
    }
    return 0;
}

int main(void)
{
    int rc = run_correctness();
    if (rc)
        return rc;
    rc = run_parity();
    if (rc)
        return rc;
    return 0;
}
