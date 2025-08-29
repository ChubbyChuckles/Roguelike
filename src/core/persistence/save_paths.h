#ifndef ROGUE_SAVE_PATHS_H
#define ROGUE_SAVE_PATHS_H

#include <stdint.h>

/* Build canonical paths for save-related files. */
const char* rogue_build_slot_path(int slot);
const char* rogue_build_autosave_path(int logical);
const char* rogue_build_backup_path(int slot, uint32_t ts);

#endif /* ROGUE_SAVE_PATHS_H */
