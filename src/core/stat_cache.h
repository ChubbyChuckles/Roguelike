/* Derived Stat Caching (14.3) */
#ifndef ROGUE_STAT_CACHE_H
#define ROGUE_STAT_CACHE_H

#include "entities/player.h"

typedef struct RogueStatCache {
    int total_strength;
    int total_dexterity;
    int total_vitality;
    int total_intelligence;
    int dps_estimate; /* simple heuristic */
    int ehp_estimate; /* effective HP proxy */
    int dirty;        /* non-zero when cache invalid */
} RogueStatCache;

extern RogueStatCache g_player_stat_cache;

void rogue_stat_cache_mark_dirty(void);
void rogue_stat_cache_update(const RoguePlayer* p); /* no-op if not dirty */

#endif
