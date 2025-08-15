#ifndef ROGUE_PERSISTENCE_H
#define ROGUE_PERSISTENCE_H

/* Persistence module: load & save generation parameters and player stats */

/* Load generation params (sets defaults first, then overwrites if file present) */
void rogue_persistence_load_generation_params(void);
/* Save generation params if dirty flag set */
void rogue_persistence_save_generation_params_if_dirty(void);

/* Load player stats (level/xp/attributes, hp/mp, unspent) */
void rogue_persistence_load_player_stats(void);
/* Save player stats unconditionally */
void rogue_persistence_save_player_stats(void);

/* Phase M3.7: persistence version tag accessors */
int rogue_persistence_player_version(void); /* returns loaded player stats version (>=1) */
int rogue_persistence_gen_params_version(void); /* returns loaded gen params version (>=1) */

/* Optional: override file paths (NULL keeps previous). Useful for tests to avoid clobbering real data. */
void rogue_persistence_set_paths(const char* player_stats_path, const char* gen_params_path);

/* Convenience: load both gen params + player stats */
static inline void rogue_persistence_init_and_load(void){
    rogue_persistence_load_generation_params();
    rogue_persistence_load_player_stats();
}
/* Convenience: save dirty gen params + player stats */
static inline void rogue_persistence_save_on_shutdown(void){
    rogue_persistence_save_generation_params_if_dirty();
    rogue_persistence_save_player_stats();
}

#endif /* ROGUE_PERSISTENCE_H */
