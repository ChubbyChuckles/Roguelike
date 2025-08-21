/* enemy_difficulty.c - Phase 0 implementation (taxonomy + tier multipliers)
 * Roadmap Coverage: 0.1 - 0.5
 */
#include "core/enemy/enemy_difficulty.h"
#include <string.h>

/* Static archetype names */
static const char* g_archetype_names[ROGUE_ENEMY_ARCHETYPE__COUNT] = {"Melee", "Ranged", "Caster",
                                                                      "EliteSupport", "Boss"};

/* Phase 0 Tier Table
 * Multipliers chosen with simple monotonic escalation; future phases may adjust.
 * Rationale (initial guess):
 *  - Veteran: modest +25% HP, +15% DPS gives slight pressure without large TTK variance.
 *  - Elite: +85% HP to anchor longer presence, +60% DPS, +40% control/mobility for richer behavior.
 *  - MiniBoss: 3.2x HP (noticeable endurance), 2.2x DPS (threat), +130% control & mobility for
 * pattern variety.
 *  - Boss: 8x HP baseline, 3.2x DPS; control +180%, mobility +170% for arena dynamics.
 *  - Nemesis: Slightly above Boss (HP 8.5x) but *less* DPS than pure boss (3.0x) reserving space
 * for adaptive scaling.
 */
static const RogueEnemyTierDesc g_tiers[] = {
    {ROGUE_ENEMY_TIER_NORMAL, "Normal", {1.00f, 1.00f, 1.00f, 1.00f}, 0u},
    {ROGUE_ENEMY_TIER_VETERAN, "Veteran", {1.25f, 1.15f, 1.10f, 1.05f}, 0u},
    {ROGUE_ENEMY_TIER_ELITE, "Elite", {1.85f, 1.60f, 1.40f, 1.40f}, 0u},
    {ROGUE_ENEMY_TIER_MINIBOSS, "MiniBoss", {3.20f, 2.20f, 2.30f, 2.30f}, 0u},
    {ROGUE_ENEMY_TIER_BOSS, "Boss", {8.00f, 3.20f, 2.80f, 2.70f}, 0u},
    {ROGUE_ENEMY_TIER_NEMESIS, "Nemesis", {8.50f, 3.00f, 3.00f, 2.90f}, 0u}};
static const int g_tier_count = (int) (sizeof(g_tiers) / sizeof(g_tiers[0]));

/* Public API */
int rogue_enemy_tier_count(void) { return g_tier_count; }

const RogueEnemyTierDesc* rogue_enemy_tier_get_by_index(int index)
{
    if (index < 0 || index >= g_tier_count)
        return NULL;
    return &g_tiers[index];
}

const RogueEnemyTierDesc* rogue_enemy_tier_get(int id)
{
    for (int i = 0; i < g_tier_count; i++)
        if (g_tiers[i].id == id)
            return &g_tiers[i];
    return NULL;
}

int rogue_enemy_difficulty_compute_base_budgets(int tier_id, RogueEnemyDifficultyBudgets* out)
{
    if (!out)
        return -2;
    const RogueEnemyTierDesc* t = rogue_enemy_tier_get(tier_id);
    if (!t)
        return -1;
    *out = t->mult; /* direct copy for Phase 0 */
    return 0;
}

int rogue_enemy_archetype_count(void) { return ROGUE_ENEMY_ARCHETYPE__COUNT; }

const char* rogue_enemy_archetype_name(int archetype)
{
    if (archetype < 0 || archetype >= ROGUE_ENEMY_ARCHETYPE__COUNT)
        return NULL;
    return g_archetype_names[archetype];
}

void rogue_enemy_difficulty_reset(void) { /* Phase 0 static tables -> nothing to do */ }

/* Internal validation (can be invoked by unit test for monotonicity & uniqueness) */
static int _validate_unique_ids(void)
{
    for (int i = 0; i < g_tier_count; i++)
    {
        for (int j = i + 1; j < g_tier_count; j++)
        {
            if (g_tiers[i].id == g_tiers[j].id)
                return -1;
        }
    }
    return 0;
}
static int _validate_monotonic(void)
{
    /* Expect non-decreasing hp & dps from one tier to the next (Nemesis slight DPS drop is
       intentional: allow adaptive headroom). We'll enforce: hp monotonic strictly increasing; dps
       mostly increasing with at most one allowed dip (Nemesis). */
    float prev_hp = -1.f;
    int dps_drop_allowed = 1;
    float prev_dps = -1.f;
    for (int i = 0; i < g_tier_count; i++)
    {
        const RogueEnemyTierDesc* t = &g_tiers[i];
        if (t->mult.hp_budget <= prev_hp)
            return -2; /* strict */
        prev_hp = t->mult.hp_budget;
        if (t->mult.dps_budget < prev_dps)
        {
            if (dps_drop_allowed)
            {
                dps_drop_allowed = 0; /* consume allowance */
            }
            else
                return -3; /* multiple dips */
        }
        else
        {
            prev_dps = t->mult.dps_budget;
        }
    }
    return 0;
}

/* Expose validation helpers for tests */
int rogue_enemy_difficulty__test_validate_ids(void) { return _validate_unique_ids(); }
int rogue_enemy_difficulty__test_validate_monotonic(void) { return _validate_monotonic(); }

/* Forward declarations for test functions (not in public header intentionally). */
extern int rogue_enemy_difficulty__test_validate_ids(void);
extern int rogue_enemy_difficulty__test_validate_monotonic(void);
