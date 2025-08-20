#ifndef ROGUE_CORE_ENEMY_INTEGRATION_H
#define ROGUE_CORE_ENEMY_INTEGRATION_H
#include "entities/enemy.h"

/* Forward declaration */
struct RogueDungeonRoom;
struct RogueEncounterUnit;

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

/* ================= Phase 2: Encounter Template → Room Placement =================
 * Functions to integrate encounter composition into dungeon room spawning.
 */

/* Room metadata structure for integration */
typedef struct RogueRoomEncounterInfo {
    int room_id;
    int depth_level;              /* room depth for difficulty calculation */
    int biome_id;                 /* biome type for template weighting */
    int encounter_template_id;    /* selected template id (-1 if none) */
    unsigned int encounter_seed;  /* computed seed for this encounter */
    int encounter_index;          /* index within room (usually 0) */
} RogueRoomEncounterInfo;

/* Choose encounter template id for a room based on depth, biome, and seed */
int rogue_enemy_integration_choose_template(int room_depth, int biome_id, unsigned int seed, int* out_template_id);

/* Compute difficulty rating for a room based on its depth and properties */
int rogue_enemy_integration_compute_room_difficulty(int room_depth, int room_area, int room_tags);

/* Generate encounter info for a room (combines template selection and seed derivation) */
int rogue_enemy_integration_prepare_room_encounter(const struct RogueDungeonRoom* room, int world_seed, int region_id, RogueRoomEncounterInfo* out_info);

/* Validate that a template can be placed in a room (space requirements, etc.) */
int rogue_enemy_integration_validate_template_placement(int template_id, const struct RogueDungeonRoom* room);

/* ================= Phase 3: Stat & Modifier Application at Spawn =================
 * Functions to apply final stats and modifiers to spawned enemies.
 */

/* Apply stats and modifiers to a composed encounter unit */
int rogue_enemy_integration_apply_unit_stats(struct RogueEnemy* enemy, const struct RogueEncounterUnit* unit, int player_level, const RogueEnemyTypeMapping* type_mapping);

/* Apply modifiers to an enemy based on encounter composition rules */
int rogue_enemy_integration_apply_unit_modifiers(struct RogueEnemy* enemy, const struct RogueEncounterUnit* unit, unsigned int modifier_seed, int is_elite, int is_boss);

/* Complete spawn processing: stats + modifiers + encounter metadata */
int rogue_enemy_integration_finalize_spawn(struct RogueEnemy* enemy, const struct RogueEncounterUnit* unit, const RogueRoomEncounterInfo* encounter_info, int player_level, const RogueEnemyTypeMapping* type_mapping);

/* Validation: check stat non-negativity and budget adherence after modifier application */
int rogue_enemy_integration_validate_final_stats(const struct RogueEnemy* enemy);

#ifdef __cplusplus
}
#endif
#endif
