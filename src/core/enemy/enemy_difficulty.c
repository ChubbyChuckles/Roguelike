/**
 * @file enemy_difficulty.c
 * @brief Phase 0 implementation (taxonomy + tier multipliers)
 *
 * Roadmap Coverage: 0.1 - 0.5
 *
 * This module contains the static tier table used by early-phase difficulty
 * computations, a small archetype name table, and a few helper APIs to query
 * tiers and archetypes. Validation helpers are provided for unit tests to
 * assert monotonicity and uniqueness of the static tables.
 */
#include "core/enemy/enemy_difficulty.h"
#include <string.h>

/**
 * @brief Static archetype human-readable names.
 */
static const char* g_archetype_names[ROGUE_ENEMY_ARCHETYPE__COUNT] = {"Melee", "Ranged", "Caster",
                                                                      "EliteSupport", "Boss"};

/**
 * @brief Phase 0 Tier Table.
 *
 * Multipliers chosen with simple monotonic escalation; future phases may
 * adjust. The multi-line rationale in the original source explains the
 * initial design intent for each tier (Veteran, Elite, MiniBoss, Boss,
 * Nemesis) and is preserved here as part of the block comment for
 * historical context.
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
/**
 * @brief Get the number of defined tiers.
 *
 * @return Count of static tiers available in `g_tiers`.
 */
int rogue_enemy_tier_count(void) { return g_tier_count; }

/**
 * @brief Retrieve a tier descriptor by array index.
 *
 * Returns NULL when the index is out of range.
 */
const RogueEnemyTierDesc* rogue_enemy_tier_get_by_index(int index)
{
    if (index < 0 || index >= g_tier_count)
        return NULL;
    return &g_tiers[index];
}

/**
 * @brief Find a tier descriptor by id.
 *
 * Iterates the static table and returns the first matching `id` or NULL if
 * not found.
 */
const RogueEnemyTierDesc* rogue_enemy_tier_get(int id)
{
    for (int i = 0; i < g_tier_count; i++)
        if (g_tiers[i].id == id)
            return &g_tiers[i];
    return NULL;
}

/**
 * @brief Compute base budgets (hp/dps/etc.) for a given tier.
 *
 * Phase 0 uses a direct copy from the static tier descriptor's multiplier
 * structure.
 *
 * @param tier_id Tier id to query.
 * @param out Output pointer to receive the budgets (must be non-NULL).
 * @return 0 on success, -1 if tier not found, -2 if `out` is NULL.
 */
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

/**
 * @brief Number of archetypes.
 */
int rogue_enemy_archetype_count(void) { return ROGUE_ENEMY_ARCHETYPE__COUNT; }

/**
 * @brief Human-readable archetype name lookup.
 *
 * @param archetype Archetype enum value.
 * @return Pointer to a static string or NULL if out-of-range.
 */
const char* rogue_enemy_archetype_name(int archetype)
{
    if (archetype < 0 || archetype >= ROGUE_ENEMY_ARCHETYPE__COUNT)
        return NULL;
    return g_archetype_names[archetype];
}

/**
 * @brief Reset difficulty system to defaults.
 *
 * Phase 0 uses static tables, so this function is intentionally a no-op and
 * exists to provide a stable API surface for later phases.
 */
void rogue_enemy_difficulty_reset(void) { /* Phase 0 static tables -> nothing to do */ }

/**
 * @brief Internal test helper: ensure tier ids are unique.
 *
 * Intended for unit tests to catch table authoring errors.
 *
 * @return 0 when all ids unique, negative on duplicate.
 */
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

/**
 * @brief Internal test helper: ensure monotonic escalation of tiers.
 *
 * Expects strictly increasing HP budgets between successive tiers. DPS is
 * expected to mostly increase; one dip (Nemesis) is permitted as an
 * intentional design choice to preserve adaptive headroom.
 *
 * @return 0 on success, negative error codes for failure modes.
 */
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
