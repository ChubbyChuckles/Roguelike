/* hud_bars.c - UI Phase 6.2 layered HUD bars implementation */
#include "hud_bars.h"

/**
 * @brief Clamps a float value to the range [0.0, 1.0].
 *
 * This is a utility function used internally to ensure bar fractions
 * stay within valid display bounds.
 *
 * @param v The input float value to clamp.
 * @return The clamped value between 0.0 and 1.0 inclusive.
 */
static float clamp01(float v)
{
    if (v < 0)
        return 0.0f;
    if (v > 1)
        return 1.0f;
    return v;
}

/**
 * @brief Resets the HUD bars state to full values.
 *
 * Initializes or resets all bar values to 1.0 (full) and marks the state
 * as initialized. This is typically called when starting a new game or
 * when the player respawns.
 *
 * @param st Pointer to the RogueHUDBarsState structure to reset.
 *           If NULL, the function returns immediately without error.
 */
void rogue_hud_bars_reset(RogueHUDBarsState* st)
{
    if (!st)
        return;
    st->health_primary = st->health_secondary = 1.0f;
    st->mana_primary = st->mana_secondary = 1.0f;
    st->ap_primary = st->ap_secondary = 1.0f;
    st->initialized = 1;
}

/**
 * @brief Updates the HUD bars state with new player values.
 *
 * This function implements the core smoothing logic for the layered bars:
 * - Primary values are set to current fractions immediately
 * - Secondary values lag behind on decreases (damage taken) and catch up
 *   at a fixed rate, while snapping forward immediately on increases (heals)
 *
 * @param st Pointer to the RogueHUDBarsState structure to update.
 *           If NULL, the function returns immediately without error.
 * @param hp Current health points value.
 * @param hp_max Maximum health points value.
 * @param mp Current mana points value.
 * @param mp_max Maximum mana points value.
 * @param ap Current action points value.
 * @param ap_max Maximum action points value.
 * @param dt_ms Time delta in milliseconds since last update, used for
 *              smoothing calculations. If <= 0, no smoothing occurs.
 */
void rogue_hud_bars_update(RogueHUDBarsState* st, int hp, int hp_max, int mp, int mp_max, int ap,
                           int ap_max, int dt_ms)
{
    if (!st)
        return;
    if (!st->initialized)
    {
        rogue_hud_bars_reset(st);
    }
    float hp_p = (hp_max > 0) ? (float) hp / (float) hp_max : 0.0f;
    float mp_p = (mp_max > 0) ? (float) mp / (float) mp_max : 0.0f;
    float ap_p = (ap_max > 0) ? (float) ap / (float) ap_max : 0.0f;
    if (hp_p < 0)
        hp_p = 0;
    if (hp_p > 1)
        hp_p = 1;
    if (mp_p < 0)
        mp_p = 0;
    if (mp_p > 1)
        mp_p = 1;
    if (ap_p < 0)
        ap_p = 0;
    if (ap_p > 1)
        ap_p = 1;
    /* Snap secondary upward instantly (heals); lag downward at fixed rate fraction/sec. */
    const float catchup_rate = 1.5f; /* fraction units per second */
    float dt = (dt_ms <= 0) ? 0.0f : (dt_ms / 1000.0f);
    /* Health */
    if (hp_p >= st->health_secondary)
        st->health_secondary = hp_p;
    else
    {
        float drop = catchup_rate * dt;
        if (st->health_secondary - drop < hp_p)
            st->health_secondary = hp_p;
        else
            st->health_secondary -= drop;
    }
    st->health_primary = hp_p;
    /* Mana */
    if (mp_p >= st->mana_secondary)
        st->mana_secondary = mp_p;
    else
    {
        float drop = catchup_rate * dt;
        if (st->mana_secondary - drop < mp_p)
            st->mana_secondary = mp_p;
        else
            st->mana_secondary -= drop;
    }
    st->mana_primary = mp_p;
    /* AP */
    if (ap_p >= st->ap_secondary)
        st->ap_secondary = ap_p;
    else
    {
        float drop = catchup_rate * dt;
        if (st->ap_secondary - drop < ap_p)
            st->ap_secondary = ap_p;
        else
            st->ap_secondary -= drop;
    }
    st->ap_primary = ap_p;
}

/**
 * @brief Gets the current (primary) health bar fraction.
 *
 * Returns the instantaneous health fraction clamped to [0.0, 1.0].
 * This represents the current health state and is used for the
 * foreground bar in layered HUD displays.
 *
 * @param st Pointer to the RogueHUDBarsState structure. If NULL, returns 0.0.
 * @return Current health fraction between 0.0 and 1.0.
 */
float rogue_hud_health_primary(const RogueHUDBarsState* st)
{
    return clamp01(st ? st->health_primary : 0.0f);
}

/**
 * @brief Gets the secondary (lagging) health bar fraction.
 *
 * Returns the smoothed health fraction that lags behind the primary
 * value on decreases. This creates the "damage taken" visual effect
 * and is used for the background bar in layered HUD displays.
 *
 * @param st Pointer to the RogueHUDBarsState structure. If NULL, returns 0.0.
 * @return Secondary health fraction between 0.0 and 1.0.
 */
float rogue_hud_health_secondary(const RogueHUDBarsState* st)
{
    return clamp01(st ? st->health_secondary : 0.0f);
}

/**
 * @brief Gets the current (primary) mana bar fraction.
 *
 * Returns the instantaneous mana fraction clamped to [0.0, 1.0].
 * This represents the current mana state and is used for the
 * foreground bar in layered HUD displays.
 *
 * @param st Pointer to the RogueHUDBarsState structure. If NULL, returns 0.0.
 * @return Current mana fraction between 0.0 and 1.0.
 */
float rogue_hud_mana_primary(const RogueHUDBarsState* st)
{
    return clamp01(st ? st->mana_primary : 0.0f);
}

/**
 * @brief Gets the secondary (lagging) mana bar fraction.
 *
 * Returns the smoothed mana fraction that lags behind the primary
 * value on decreases. This creates the "resource depletion" visual effect
 * and is used for the background bar in layered HUD displays.
 *
 * @param st Pointer to the RogueHUDBarsState structure. If NULL, returns 0.0.
 * @return Secondary mana fraction between 0.0 and 1.0.
 */
float rogue_hud_mana_secondary(const RogueHUDBarsState* st)
{
    return clamp01(st ? st->mana_secondary : 0.0f);
}

/**
 * @brief Gets the current (primary) action points bar fraction.
 *
 * Returns the instantaneous action points fraction clamped to [0.0, 1.0].
 * This represents the current AP state and is used for the
 * foreground bar in layered HUD displays.
 *
 * @param st Pointer to the RogueHUDBarsState structure. If NULL, returns 0.0.
 * @return Current action points fraction between 0.0 and 1.0.
 */
float rogue_hud_ap_primary(const RogueHUDBarsState* st)
{
    return clamp01(st ? st->ap_primary : 0.0f);
}

/**
 * @brief Gets the secondary (lagging) action points bar fraction.
 *
 * Returns the smoothed action points fraction that lags behind the primary
 * value on decreases. This creates the "AP expenditure" visual effect
 * and is used for the background bar in layered HUD displays.
 *
 * @param st Pointer to the RogueHUDBarsState structure. If NULL, returns 0.0.
 * @return Secondary action points fraction between 0.0 and 1.0.
 */
float rogue_hud_ap_secondary(const RogueHUDBarsState* st)
{
    return clamp01(st ? st->ap_secondary : 0.0f);
}
