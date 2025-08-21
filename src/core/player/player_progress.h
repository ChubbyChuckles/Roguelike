#ifndef ROGUE_PLAYER_PROGRESS_H
#define ROGUE_PLAYER_PROGRESS_H

/* Update player progression each frame: level ups, difficulty scalar,
   passive regen (health/mana), level-up aura timing, and autosave of stats. */
void rogue_player_progress_update(double dt_seconds);

#endif /* ROGUE_PLAYER_PROGRESS_H */
