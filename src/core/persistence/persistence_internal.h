/* Internal shared declarations for persistence module split */
#ifndef ROGUE_PERSISTENCE_INTERNAL_H
#define ROGUE_PERSISTENCE_INTERNAL_H

#include <stdio.h>

/* Expose path helper accessors to split compilation units */
const char* rogue__player_stats_path(void);
const char* rogue__gen_params_path(void);

/* Shared path buffers (defined in persistence_io.c) */
extern char g_player_stats_path[260];
extern char g_gen_params_path[260];

#endif /* ROGUE_PERSISTENCE_INTERNAL_H */
