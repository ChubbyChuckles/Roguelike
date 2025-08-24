#ifndef ROGUE_VFX_CONFIG_H
#define ROGUE_VFX_CONFIG_H

#include <stdint.h>

/* Load VFX definitions from a CSV config file.
   Expected columns (header optional):
     id, layer, lifetime_ms, world_space, emit_hz, particle_lifetime_ms, max_particles
   - layer can be 0..3 or BG/MID/FG/UI (case-insensitive)
   Returns 0 on success. On success, out_loaded_count receives number of entries loaded. */
int rogue_vfx_load_cfg(const char* filename, int* out_loaded_count);

/* Register a hot-reload watcher for a VFX config file. When the file changes,
   it is reloaded via rogue_vfx_load_cfg. Returns 0 on success. */
int rogue_vfx_config_watch(const char* filename);

/* Validation error reporting from the last rogue_vfx_load_cfg call. */
int rogue_vfx_last_cfg_error_count(void);
int rogue_vfx_last_cfg_error_get(int index, char* out, unsigned out_sz);

#endif /* ROGUE_VFX_CONFIG_H */
