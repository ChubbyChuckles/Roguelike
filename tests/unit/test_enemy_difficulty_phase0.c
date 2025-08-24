/* test_enemy_difficulty_phase0.c - Roadmap Phase 0 tests (0.1 - 0.5)
 * Verifies:
 *  - Tier table count & expected ordering
 *  - Unique tier ids
 *  - Monotonic HP multipliers; DPS monotonic except allowed Nemesis dip rule
 *  - Archetype enumeration integrity
 *  - Base budget fetch matches table
 */
#include "../../src/core/enemy/enemy_difficulty.h"
#include <stdio.h>

/* Forward declare internal validation helpers (not part of public API) */
int rogue_enemy_difficulty__test_validate_ids(void);
int rogue_enemy_difficulty__test_validate_monotonic(void);

static int float_eq(float a, float b)
{
    float d = a - b;
    if (d < 0)
        d = -d;
    return d < 0.0001f;
}

int main(void)
{
    /* Basic counts */
    if (rogue_enemy_tier_count() < 6)
    {
        printf("FAIL tier count\n");
        return 1;
    }
    /* Unique ids */
    if (rogue_enemy_difficulty__test_validate_ids() != 0)
    {
        printf("FAIL tier id uniqueness\n");
        return 1;
    }
    if (rogue_enemy_difficulty__test_validate_monotonic() != 0)
    {
        printf("FAIL tier monotonic\n");
        return 1;
    }
    /* Sample tier pick */
    const RogueEnemyTierDesc* boss = rogue_enemy_tier_get(ROGUE_ENEMY_TIER_BOSS);
    if (!boss)
    {
        printf("FAIL boss tier lookup\n");
        return 1;
    }
    if (boss->mult.hp_budget < 7.5f || boss->mult.dps_budget < 3.0f)
    {
        printf("FAIL boss multiplier bounds\n");
        return 1;
    }
    RogueEnemyDifficultyBudgets out;
    if (rogue_enemy_difficulty_compute_base_budgets(ROGUE_ENEMY_TIER_ELITE, &out) != 0)
    {
        printf("FAIL compute budgets\n");
        return 1;
    }
    const RogueEnemyTierDesc* elite = rogue_enemy_tier_get(ROGUE_ENEMY_TIER_ELITE);
    if (!elite || !float_eq(out.hp_budget, elite->mult.hp_budget))
    {
        printf("FAIL elite hp budget mismatch\n");
        return 1;
    }
    /* Archetypes */
    if (rogue_enemy_archetype_count() != ROGUE_ENEMY_ARCHETYPE__COUNT)
    {
        printf("FAIL archetype count\n");
        return 1;
    }
    for (int i = 0; i < rogue_enemy_archetype_count(); i++)
    {
        const char* n = rogue_enemy_archetype_name(i);
        if (!n || !n[0])
        {
            printf("FAIL archetype name %d\n", i);
            return 1;
        }
    }
    printf("OK test_enemy_difficulty_phase0 (%d tiers, %d archetypes)\n", rogue_enemy_tier_count(),
           rogue_enemy_archetype_count());
    return 0;
}
