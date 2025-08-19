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

/* ================= Phase 1: Spawn Seed Derivation & Determinism =================
 * encounter_seed = world_seed ^ region_id ^ room_id ^ encounter_index (simple XOR per roadmap)
 * Provides deterministic RNG stream separation for future: composition/modifiers/ai.
 */
unsigned int rogue_enemy_integration_encounter_seed(unsigned int world_seed, int region_id, int room_id, int encounter_index);

/* Replay hash: hash(template_id + unit levels + (sorted) modifier ids placeholder) producing 64-bit summary.
 * Modifiers not yet integrated (Phase 3); pass modifier_ids=NULL, modifier_count=0 for now.
 */
unsigned long long rogue_enemy_integration_replay_hash(int template_id, const int* unit_levels, int unit_count, const int* modifier_ids, int modifier_count);

/* Record an encounter determinism snapshot (seed + hash) into a small ring buffer (last 32). */
void rogue_enemy_integration_debug_record(unsigned int seed, unsigned long long hash, int template_id, int unit_count);

/* Dump last N (<=32) encounter records to provided buffer as lines: idx seed hash template unit_count. Returns bytes written. */
int rogue_enemy_integration_debug_dump(char* buf, int buf_size);

#ifdef __cplusplus
}
#endif
#endif
