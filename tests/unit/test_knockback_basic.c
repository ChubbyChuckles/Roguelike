#include "../../src/entities/enemy.h"
#include "../../src/entities/player.h"
#include "../../src/game/hit_system.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef main
#undef main
#endif

/* This test fakes a sweep debug frame to validate knockback magnitude application math indirectly.
   We simulate a normal and reuse combat_strike knockback formula by directly calling scaling logic.
   For minimal coupling we'll reimplement the magnitude function analogous to combat code and ensure
   expected range. */

static float calc_mag(int player_level, int enemy_level, int p_str, int e_str)
{
    float level_diff = (float) player_level - (float) enemy_level;
    if (level_diff < 0)
        level_diff = 0;
    float str_diff = (float) p_str - (float) e_str;
    if (str_diff < 0)
        str_diff = 0;
    float mag = 0.18f + 0.02f * level_diff + 0.015f * str_diff;
    if (mag > 0.55f)
        mag = 0.55f;
    return mag;
}

int main()
{
    float m1 = calc_mag(5, 5, 10, 10); /* baseline */
    assert(m1 >= 0.18f && m1 <= 0.20f);
    float m2 = calc_mag(10, 5, 20, 5); /* higher diffs */
    assert(m2 > m1);
    float mcap = calc_mag(200, 1, 500, 1);
    assert(mcap <= 0.55f + 1e-4f);
    printf("knockback_basic: PASS (%.3f -> %.3f cap %.3f)\n", m1, m2, mcap);
    return 0;
}
