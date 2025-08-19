#ifndef ROGUE_CORE_ENEMY_INTEGRATION_H
#define ROGUE_CORE_ENEMY_INTEGRATION_H
#include "entities/enemy.h"
#ifdef __cplusplus
extern "C" { 
#endif

/* Mapping entry linking type index to difficulty metadata (Phase 0.2) */
typedef struct RogueEnemyTypeMapping {
    int type_index;          /* index into g_app.enemy_types */
    int archetype_id;
    int tier_id;
    int base_level_offset;
    char id[32];
    char name[32];
} RogueEnemyTypeMapping;

int rogue_enemy_integration_build_mappings(RogueEnemyTypeMapping* out, int max, int* out_count);
int rogue_enemy_integration_find_by_type(int type_index, const RogueEnemyTypeMapping* arr, int count);
int rogue_enemy_integration_validate_unique(const RogueEnemyTypeMapping* arr, int count);

/* Apply initial integration fields to a runtime enemy (Phase 0.3/0.4 basic) */
void rogue_enemy_integration_apply_spawn(struct RogueEnemy* e, const RogueEnemyTypeMapping* map_entry, int player_level);

#ifdef __cplusplus
}
#endif
#endif
