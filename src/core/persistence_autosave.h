#ifndef ROGUE_PERSISTENCE_AUTOSAVE_H
#define ROGUE_PERSISTENCE_AUTOSAVE_H
/* Decoupled autosave tick (was inside player_progress). */
void rogue_persistence_autosave_update(double dt_seconds);
void rogue_persistence_autosave_force(void);
#endif
