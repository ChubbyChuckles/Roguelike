#ifndef ROGUE_PLAYER_PROGRESS_H
#define ROGUE_PLAYER_PROGRESS_H

/* Update player progression each frame: level ups, difficulty scalar,
   passive regen (health/mana), level-up aura timing, and autosave of stats. */
void rogue_player_progress_update(double dt_seconds);

/* Phase 2.4: Overdrive & Exhaustion API
    Temporarily raises the AP cap by ap_bonus for duration_ms; when it ends, applies
    an AP regen throttle (exhaustion) for exhaustion_ms. */
void rogue_overdrive_begin(int ap_bonus, float duration_ms, float exhaustion_ms);

/* Phase 2.5: Heat loop utilities
    Adds heat and triggers Overheat when reaching max. Venting is handled in the
    regular update loop. */
void rogue_heat_add(int amount);

#endif /* ROGUE_PLAYER_PROGRESS_H */
