/* hud_bars.h - UI Phase 6.2 layered HUD bars (health/mana/AP) with secondary lag fill
   Provides stateful smoothing for a trailing (damage taken) bar and simple API returning
   instantaneous (primary) and smoothed (secondary) fractions suitable for rendering with
   two colored rectangles. The smoothing model:
       - Primary fraction = current_value / max_value (clamped 0..1)
       - Secondary lags behind on decreases (damage) and catches up at a configured rate
       - On increases (heals/regeneration) secondary snaps immediately to primary
   This matches common ARPG health bar behavior.
   Test harness will tick update with synthetic value changes and assert secondary lag.
*/
#ifndef ROGUE_HUD_BARS_H
#define ROGUE_HUD_BARS_H

typedef struct RogueHUDBarsState
{
    float health_primary, health_secondary;
    float mana_primary, mana_secondary;
    float ap_primary, ap_secondary;
    int initialized;
} RogueHUDBarsState;

/* Initialize / reset bar state (secondary = primary). */
void rogue_hud_bars_reset(RogueHUDBarsState* st);

/* Update bar state given current raw values. dt_ms used for smoothing progression. */
void rogue_hud_bars_update(RogueHUDBarsState* st, int hp, int hp_max, int mp, int mp_max, int ap,
                           int ap_max, int dt_ms);

/* Accessors clamp to [0,1]. */
float rogue_hud_health_primary(const RogueHUDBarsState* st);
float rogue_hud_health_secondary(const RogueHUDBarsState* st);
float rogue_hud_mana_primary(const RogueHUDBarsState* st);
float rogue_hud_mana_secondary(const RogueHUDBarsState* st);
float rogue_hud_ap_primary(const RogueHUDBarsState* st);
float rogue_hud_ap_secondary(const RogueHUDBarsState* st);

#endif /* ROGUE_HUD_BARS_H */
