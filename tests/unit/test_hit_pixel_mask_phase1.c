/* test_hit_pixel_mask_phase1.c - validates Phase 1 minimal pixel mask loader integration */
#include "../../src/game/hit_pixel_mask.h"
#include "../../src/game/pixel_mask_loader.h"
#include <stdio.h>
#include <string.h>

static int assert_true(int expr, const char* msg)
{
    if (!expr)
    {
        printf("FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

int main(void)
{
    /* Ensure placeholder set (may load real asset if available) */
    RogueHitPixelMaskSet* set = rogue_hit_pixel_masks_ensure(0);
    if (!assert_true(set != NULL, "ensure returns set"))
        return 1;
    if (!assert_true(set->ready == 1, "set ready"))
        return 1;
    if (!assert_true(set->frame_count == 8, "frame_count 8"))
        return 1;
    int any_bits = 0;
    int w = 0, h = 0;
    rogue_hit_mask_frame_aabb(&set->frames[0], &w, &h);
    if (!assert_true(w > 0 && h > 0, "frame dims >0"))
        return 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            if (rogue_hit_mask_test(&set->frames[0], x, y))
            {
                any_bits = 1;
                break;
            }
        }
    if (!assert_true(any_bits, "at least one solid pixel"))
        return 1;
    /* Enemy test sanity: center or ring should hit somewhere for placeholder */
    int lx = -1, ly = -1;
    int hit = rogue_hit_mask_enemy_test(&set->frames[0], (float) (w / 2), (float) (h / 2), 4.0f,
                                        &lx, &ly);
    if (!assert_true(hit == 1, "enemy test reports hit"))
        return 1;
    if (!assert_true(lx >= 0 && ly >= 0, "reported local impact coords"))
        return 1;
    /* Reset and ensure re-acquire still works */
    rogue_hit_pixel_masks_reset_all();
    set = rogue_hit_pixel_masks_ensure(0);
    if (!assert_true(set && set->ready, "ensure after reset"))
        return 1;
    printf("OK test_hit_pixel_mask_phase1\n");
    return 0;
}
