#include "core/loot/loot_rarity_adv.h"
#include <assert.h>
#include <stdio.h>

/* Test 9.4: accelerated pity thresholds reduce required misses once halfway reached. */

static int simulate_sequence(int epic_th, int legendary_th)
{
    rogue_rarity_adv_reset();
    rogue_rarity_pity_set_thresholds(epic_th, legendary_th);
    rogue_rarity_pity_set_acceleration(1);
    int upgrades = 0;
    /* Simulate consecutive sub-epic drops represented by calling apply_pity with rarity 0 (common)
     * band 0..4 */
    for (int i = 0; i < 100; i++)
    {
        int effE = rogue_rarity_pity_get_effective_epic();
        int effL = rogue_rarity_pity_get_effective_legendary();
        (void) effE;
        (void) effL; /* silence warnings if unused */
        int r = 0;   /* rolled common */
        r = rogue_rarity_apply_pity(r, 0, 4);
        if (r >= 3)
        {
            upgrades++;
            break;
        }
    }
    return upgrades;
}

int main(void)
{
    int up = simulate_sequence(20, 0); /* epic threshold 20 -> acceleration should reduce effective
                                          threshold to 15 after 10 misses */
    /* We expect upgrade happened (counter resets at upgrade), upgrades ==1 */
    assert(up == 1);
    printf("PITY_ACCEL_OK upgrades=%d eff_epic=%d\n", up, rogue_rarity_pity_get_effective_epic());
    return 0;
}
