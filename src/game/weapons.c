/**
 * @file weapons.c
 * @brief Implements weapon system functionality including weapon definitions, familiarity
 * progression, durability tracking, and combat stance modifiers.
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 *
 * This file provides functions to manage weapons, track weapon familiarity and durability,
 * and handle combat stance modifiers that affect weapon performance.
 */

#include "weapons.h"
#include <string.h>

/** @brief Static weapon table (could be externalized later) */
static const RogueWeaponDef g_weapon_table[] = {
    {0, "Training Sword", 8, 0.65f, 0.15f, 0.0f, 1.0f, 1.0f, 100.0f},
    {1, "Great Hammer", 18, 0.85f, 0.05f, 0.0f, 1.25f, 1.35f, 140.0f},
    {2, "Rapier", 6, 0.40f, 0.55f, 0.0f, 0.85f, 0.80f, 90.0f},
    {3, "Focus Catalyst", 4, 0.10f, 0.15f, 0.70f, 1.10f, 0.75f, 80.0f},
};
/** @brief Number of weapon definitions in the table */
static const int g_weapon_count = (int) (sizeof(g_weapon_table) / sizeof(g_weapon_table[0]));

/** @brief Capacity for familiarity array */
#define FAM_CAP 16
/** @brief Familiarity tracking array for weapons */
static RogueWeaponFamiliarity g_fam[FAM_CAP];
/** @brief Initialization flag for familiarity system */
static int g_fam_init = 0;

/** @brief Durability runtime values indexed by weapon ID */
static float g_durability[FAM_CAP]; /* index by weapon id while within range */
/** @brief Initialization flag for durability system */
static int g_durability_init = 0;

/**
 * @brief Retrieves a weapon definition by its ID.
 *
 * @param id The weapon definition ID to look up.
 * @return Pointer to the weapon definition if found, NULL otherwise.
 *
 * This function searches through the weapon table for a definition
 * matching the specified ID.
 */
const RogueWeaponDef* rogue_weapon_get(int id)
{
    if (id < 0)
        return 0;
    for (int i = 0; i < g_weapon_count; i++)
    {
        if (g_weapon_table[i].id == id)
            return &g_weapon_table[i];
    }
    return 0;
}

/**
 * @brief Gets or creates a familiarity slot for the specified weapon.
 *
 * @param weapon_id The weapon ID to get familiarity data for.
 * @return Pointer to the familiarity data, or NULL if weapon_id is invalid or no slots available.
 *
 * This function manages the familiarity tracking array, using direct indexing for weapons
 * within FAM_CAP and falling back to search-based allocation for higher IDs.
 */
static RogueWeaponFamiliarity* fam_slot(int weapon_id)
{
    if (!g_fam_init)
    {
        for (int i = 0; i < FAM_CAP; i++)
        {
            g_fam[i].weapon_id = -1;
            g_fam[i].usage_points = 0.0f;
        }
        g_fam_init = 1;
    }
    if (weapon_id < 0)
        return 0;
    /**< @brief If weapon id fits within cap, use direct slot for determinism */
    if (weapon_id < FAM_CAP)
    {
        if (g_fam[weapon_id].weapon_id == -1)
            g_fam[weapon_id].weapon_id = weapon_id;
        return &g_fam[weapon_id];
    }
    /**< @brief Else fallback: search existing, then find first empty */
    for (int i = 0; i < FAM_CAP; i++)
    {
        if (g_fam[i].weapon_id == weapon_id)
            return &g_fam[i];
    }
    for (int i = 0; i < FAM_CAP; i++)
    {
        if (g_fam[i].weapon_id == -1)
        {
            g_fam[i].weapon_id = weapon_id;
            return &g_fam[i];
        }
    }
    return 0;
}

/**
 * @brief Calculates the familiarity bonus for a weapon based on usage points.
 *
 * @param weapon_id The weapon ID to calculate bonus for.
 * @return The familiarity bonus multiplier (0.0 to 0.10).
 *
 * This function computes a damage bonus based on accumulated usage points,
 * with a soft cap at 10,000 points yielding a maximum 10% bonus.
 */
float rogue_weapon_get_familiarity_bonus(int weapon_id)
{
    RogueWeaponFamiliarity* s = fam_slot(weapon_id);
    if (!s)
        return 0.0f;
    float p = s->usage_points;
    if (p > 10000.0f)
        p = 10000.0f; /* soft cap */
    float bonus = (p / 10000.0f) * 0.10f;
    if (bonus > 0.10f)
        bonus = 0.10f;
    return bonus;
}

/**
 * @brief Registers a weapon hit to increase familiarity points.
 *
 * @param weapon_id The weapon ID that was used.
 * @param damage_done The amount of damage dealt in the hit.
 *
 * This function awards familiarity points based on damage dealt,
 * with a minimum progression even for low-damage hits to ensure
 * familiarity growth in testing scenarios.
 */
void rogue_weapon_register_hit(int weapon_id, float damage_done)
{
    RogueWeaponFamiliarity* s = fam_slot(weapon_id);
    if (!s)
        return;
    if (damage_done < 0.0f)
        damage_done = 0.0f; /**< @brief no regression */
    /**< @brief Award small base progression even for low damage so tests with tiny numbers grow */
    float inc = damage_done * 0.5f + 1.0f;
    s->usage_points += inc;
    if (s->usage_points > 10000.0f)
        s->usage_points = 10000.0f;
}

/**
 * @brief Reduces weapon durability by the specified amount.
 *
 * @param weapon_id The weapon ID to update durability for.
 * @param amount The amount of durability to subtract.
 *
 * This function decreases weapon durability, initializing it to maximum
 * if not previously set, and ensuring it doesn't go below zero.
 */
void rogue_weapon_tick_durability(int weapon_id, float amount)
{
    if (weapon_id < 0 || weapon_id >= FAM_CAP)
        return;
    if (!g_durability_init)
    {
        for (int i = 0; i < FAM_CAP; i++)
            g_durability[i] = -1.0f;
        g_durability_init = 1;
    }
    const RogueWeaponDef* wd = rogue_weapon_get(weapon_id);
    if (!wd)
        return;
    if (g_durability[weapon_id] < 0)
        g_durability[weapon_id] = wd->durability_max;
    g_durability[weapon_id] -= amount;
    if (g_durability[weapon_id] < 0)
        g_durability[weapon_id] = 0;
}

/**
 * @brief Gets the current durability of a weapon.
 *
 * @param weapon_id The weapon ID to query.
 * @return The current durability value, or 0 if invalid/uninitialized.
 *
 * This function returns the current durability level of the specified weapon.
 */
float rogue_weapon_current_durability(int weapon_id)
{
    if (weapon_id < 0 || weapon_id >= FAM_CAP)
        return 0;
    if (!g_durability_init)
        return 0;
    return g_durability[weapon_id];
}

/**
 * @brief Gets the combat stance modifiers for the specified stance.
 *
 * @param stance The stance ID (0=balanced, 1=aggressive, 2=defensive).
 * @return A structure containing damage, stamina, and poise damage multipliers.
 *
 * This function returns modifiers that affect weapon performance based on
 * the current combat stance. Aggressive stance increases damage but consumes
 * more stamina, while defensive stance reduces damage but is more conservative.
 */
RogueStanceModifiers rogue_stance_get_mods(int stance)
{
    RogueStanceModifiers m = {1.0f, 1.0f, 1.0f};
    switch (stance)
    {
    case 1: /**< @brief aggressive */
        m.damage_mult = 1.15f;
        m.stamina_mult = 1.15f;
        m.poise_damage_mult = 1.10f;
        break;
    case 2: /**< @brief defensive */
        m.damage_mult = 0.90f;
        m.stamina_mult = 0.85f;
        m.poise_damage_mult = 0.95f;
        break;
    default:
        break; /**< @brief balanced */
    }
    return m;
}

/**
 * @brief Applies stance-based adjustments to weapon attack frame timings.
 *
 * @param stance The stance ID (0=balanced, 1=aggressive, 2=defensive).
 * @param base_windup_ms Base windup time in milliseconds.
 * @param base_recover_ms Base recovery time in milliseconds.
 * @param out_windup_ms Output parameter for adjusted windup time.
 * @param out_recover_ms Output parameter for adjusted recovery time.
 *
 * This function modifies weapon attack timings based on combat stance.
 * Aggressive stance speeds up attacks, while defensive stance slows them down.
 */
void rogue_stance_apply_frame_adjustments(int stance, float base_windup_ms, float base_recover_ms,
                                          float* out_windup_ms, float* out_recover_ms)
{
    float w = base_windup_ms;
    float r = base_recover_ms;
    if (stance == 1)
    { /**< @brief aggressive */
        w *= 0.95f;
        r *= 0.97f;
    }
    else if (stance == 2)
    { /**< @brief defensive */
        w *= 1.06f;
        r *= 1.08f;
    }
    if (out_windup_ms)
        *out_windup_ms = w;
    if (out_recover_ms)
        *out_recover_ms = r;
}
