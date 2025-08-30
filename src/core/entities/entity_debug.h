#ifndef ROGUE_CORE_ENTITIES_ENTITY_DEBUG_H
#define ROGUE_CORE_ENTITIES_ENTITY_DEBUG_H

#include <stddef.h>

typedef struct RogueEntityDebugInfo
{
    int slot_index; /* index in g_app.enemies */
    int alive;      /* 1 if alive */
    int type_index; /* enemy type index */
    float x, y;     /* world tile coordinates */
    int health;
    int max_health;
} RogueEntityDebugInfo;

/* Count alive enemies. */
int rogue_entity_debug_count(void);
/* Fill out_indices with slot indices of alive enemies; returns number written. */
int rogue_entity_debug_list(int* out_indices, int cap);
/* Populate info for a given slot index; returns 0 on success. */
int rogue_entity_debug_get_info(int slot_index, RogueEntityDebugInfo* out);
/* Teleport an enemy to world coords; returns 0 on success. */
int rogue_entity_debug_teleport(int slot_index, float x, float y);
/* Kill an enemy by slot index; returns 0 on success. */
int rogue_entity_debug_kill(int slot_index);
/* Spawn a hostile enemy at player-relative offset (dx,dy). Returns slot index or -1. */
int rogue_entity_debug_spawn_at_player(float dx, float dy);

#endif
