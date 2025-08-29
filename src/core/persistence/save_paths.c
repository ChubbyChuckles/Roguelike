#include "save_internal.h"
#include <stdio.h>

static char slot_path[128];
static char autosave_path[128];
static char backup_path_buf[160];

const char* rogue_build_slot_path(int slot)
{
    snprintf(slot_path, sizeof slot_path, "save_slot_%d.sav", slot);
    return slot_path;
}

const char* rogue_build_autosave_path(int logical)
{
    int ring = logical % ROGUE_AUTOSAVE_RING;
    snprintf(autosave_path, sizeof autosave_path, "autosave_%d.sav", ring);
    return autosave_path;
}

const char* rogue_build_backup_path(int slot, uint32_t ts)
{
    snprintf(backup_path_buf, sizeof backup_path_buf, "save_slot_%d_%u.bak", slot, ts);
    return backup_path_buf;
}
