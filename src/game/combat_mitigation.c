/**
 * @file combat_mitigation.c
 * @brief Enemy damage mitigation and resistance system
 *
 * This module implements the damage mitigation system for enemies, handling:
 * - Physical damage reduction through armor and resistance
 * - Elemental damage reduction (fire, frost, arcane)
 * - True damage bypass of all mitigation
 * - Softcap mechanics for high-damage scenarios
 * - Overkill damage calculation
 *
 * The system uses a concave physical resistance curve with diminishing returns
 * to ensure increasing resistance always reduces damage while maintaining
 * practical damage caps. Elemental resistances use simpler linear reduction.
 *
 * Key features:
 * - Armor provides flat damage reduction before resistance calculations
 * - Physical resistance curve: linear up to 50%, diminishing returns to 70% max
 * - Elemental resistances: simple percentage reduction (0-90%)
 * - Softcap system prevents excessive mitigation on high-damage attacks
 * - Overkill tracking for damage exceeding enemy health
 *
 * @note Physical resist curve is concave and monotonic with diminishing returns
 * @note Softcap prevents trivializing high-damage attacks through excessive mitigation
 */

#include "combat.h"
#include <math.h>

/**
 * @brief Clamp an integer value to a specified range
 *
 * Utility function that constrains an integer value to fall within the
 * specified minimum and maximum bounds, inclusive.
 *
 * @param v Value to clamp
 * @param lo Minimum allowed value (inclusive)
 * @param hi Maximum allowed value (inclusive)
 * @return Clamped value, guaranteed to be in range [lo, hi]
 *
 * @note This is an internal utility function used by mitigation calculations
 */
static int clampi(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}
/**
 * @brief Calculate effective physical resistance with diminishing returns
 *
 * Converts raw physical resistance percentage into effective resistance using
 * a concave curve with diminishing returns. This ensures that increasing
 * resistance always reduces damage while maintaining practical caps below 80%
 * to avoid trivializing combat.
 *
 * The curve is piecewise:
 * - Linear scaling from 0% to 50% resistance
 * - Diminishing returns from 50% to 90% resistance, approaching ~70% effective
 *
 * @param p Raw physical resistance percentage (0-90)
 * @return Effective resistance percentage (0-75)
 *
 * @note Curve is concave and monotonic - increasing resistance always helps
 * @note Diminishing returns prevent excessive mitigation at high resistance values
 * @note Maximum effective resistance is capped at 75% to maintain combat viability
 * @note Input is clamped to 0-90 range internally
 */
static int rogue_effective_phys_resist(int p)
{
    if (p <= 0)
        return 0;
    if (p > 90)
        p = 90;
    /* Piecewise: linear up to 50, then diminishing so that 90 maps ~70 */
    float pf = (float) p;
    float efff = 0.0f;
    if (pf <= 50.0f)
    {
        efff = pf;
    }
    else
    {
        /* From 50 to 90, compress towards 70 using gentle slope */
        float over = pf - 50.0f;
        efff = 50.0f + over * 0.50f; /* 90 -> 50 + 40*0.5 = 70 */
    }
    if (efff < 0.0f)
        efff = 0.0f;
    if (efff > 75.0f)
        efff = 75.0f;
    return (int) floorf(efff + 0.5f);
}

/**
 * @brief Apply damage mitigation to enemy based on damage type and defenses
 *
 * Comprehensive damage mitigation function that processes incoming damage through
 * the enemy's defensive systems. Handles different damage types with appropriate
 * mitigation mechanics and includes softcap protection against excessive mitigation
 * on high-damage attacks.
 *
 * Mitigation pipeline:
 * 1. True damage bypasses all mitigation
 * 2. Physical damage: armor reduction + resistance curve + softcap
 * 3. Elemental damage: simple percentage resistance reduction
 * 4. Minimum damage floor (1 damage minimum)
 * 5. Overkill calculation for damage exceeding health
 *
 * @param e Pointer to the enemy entity (must not be NULL, must be alive)
 * @param raw Raw incoming damage value
 * @param dmg_type Damage type (ROGUE_DMG_PHYSICAL, ROGUE_DMG_FIRE, etc.)
 * @param out_overkill Output parameter for overkill damage (can be NULL)
 * @return Final mitigated damage value (minimum 1)
 *
 * @note Dead enemies take 0 damage
 * @note True damage completely bypasses armor and resistance
 * @note Physical damage uses concave resistance curve with diminishing returns
 * @note Softcap prevents excessive mitigation on high-damage attacks
 * @note Overkill is damage that exceeds remaining health
 * @note All damage types have minimum 1 damage floor
 */
int rogue_apply_mitigation_enemy(RogueEnemy* e, int raw, unsigned char dmg_type, int* out_overkill)
{
    if (!e || !e->alive)
        return 0;
    int dmg = raw;
    if (dmg < 0)
        dmg = 0;
    if (dmg_type != ROGUE_DMG_TRUE)
    {
        if (dmg_type == ROGUE_DMG_PHYSICAL)
        {
            int armor = e->armor;
            if (armor > 0)
            {
                if (armor >= dmg)
                    dmg = (dmg > 1 ? 1 : dmg);
                else
                    dmg -= armor;
            }
            int pr_raw = clampi(e->resist_physical, 0, 90);
            int pr = rogue_effective_phys_resist(pr_raw);
            if (pr > 0)
            {
                int reduce = (dmg * pr) / 100;
                dmg -= reduce;
            }
            if (raw >= ROGUE_DEF_SOFTCAP_MIN_RAW)
            {
                float armor_frac = 0.0f;
                if (armor > 0)
                {
                    armor_frac = (float) armor / (float) (raw + armor);
                    if (armor_frac > 0.90f)
                        armor_frac = 0.90f;
                }
                float total_frac = armor_frac + (float) pr / 100.0f;
                if (total_frac > 0.0f)
                {
                    if (total_frac > ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD)
                    {
                        float excess = total_frac - ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD;
                        float adjusted_excess = excess * ROGUE_DEF_SOFTCAP_SLOPE;
                        float capped_total =
                            ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD + adjusted_excess;
                        if (capped_total > ROGUE_DEF_SOFTCAP_MAX_REDUCTION)
                            capped_total = ROGUE_DEF_SOFTCAP_MAX_REDUCTION;
                        int target = (int) floorf((float) raw * (1.0f - capped_total) + 0.5f);
                        if (target < 1)
                            target = 1;
                        if (target > dmg)
                        {
                        }
                        else
                            dmg = target;
                        /* Enforce softcap floor (5% of raw) when softcap path triggers */
                        int floor_min = (int) floorf((float) raw * 0.05f + 0.5f);
                        if (dmg < floor_min)
                            dmg = floor_min;
                    }
                }
            }
        }
        else
        {
            int resist = 0;
            switch (dmg_type)
            {
            case ROGUE_DMG_FIRE:
                resist = e->resist_fire;
                break;
            case ROGUE_DMG_FROST:
                resist = e->resist_frost;
                break;
            case ROGUE_DMG_ARCANE:
                resist = e->resist_arcane;
                break;
            default:
                break;
            }
            resist = clampi(resist, 0, 90);
            if (resist > 0)
            {
                int reduce = (dmg * resist) / 100;
                dmg -= reduce;
            }
        }
    }
    if (dmg < 1)
        dmg = 1;
    int overkill = 0;
    if (e->health - dmg < 0)
    {
        overkill = dmg - e->health;
    }
    if (out_overkill)
        *out_overkill = overkill;
    return dmg;
}
