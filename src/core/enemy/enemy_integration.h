#ifndef ROGUE_CORE_ENEMY_INTEGRATION_H
#define ROGUE_CORE_ENEMY_INTEGRATION_H
#include "../../entities/enemy.h"
#include "enemy_difficulty_scaling.h"

/* Forward declaration */
struct RogueDungeonRoom;
struct RogueEncounterUnit;

#ifdef __cplusplus
extern "C"
{
#endif

    /* Mapping entry linking type index to difficulty metadata (Phase 0.2) */
    typedef struct RogueEnemyTypeMapping
    {
        int type_index; /* index into g_app.enemy_types */
        int archetype_id;
        int tier_id;
        int base_level_offset;
        char id[32];
        char name[32];
    } RogueEnemyTypeMapping;

    int rogue_enemy_integration_build_mappings(RogueEnemyTypeMapping* out, int max, int* out_count);
    int rogue_enemy_integration_find_by_type(int type_index, const RogueEnemyTypeMapping* arr,
                                             int count);
    int rogue_enemy_integration_validate_unique(const RogueEnemyTypeMapping* arr, int count);

    /* Apply initial integration fields to a runtime enemy (Phase 0.3/0.4 basic) */
    void rogue_enemy_integration_apply_spawn(struct RogueEnemy* e,
                                             const RogueEnemyTypeMapping* map_entry,
                                             int player_level);

    /* ================= Phase 1: Spawn Seed Derivation & Determinism =================
     * encounter_seed = world_seed ^ region_id ^ room_id ^ encounter_index (simple XOR per roadmap)
     * Provides deterministic RNG stream separation for future: composition/modifiers/ai.
     */
    unsigned int rogue_enemy_integration_encounter_seed(unsigned int world_seed, int region_id,
                                                        int room_id, int encounter_index);

    /* Replay hash: hash(template_id + unit levels + (sorted) modifier ids placeholder) producing
     * 64-bit summary. Modifiers not yet integrated (Phase 3); pass modifier_ids=NULL,
     * modifier_count=0 for now.
     */
    unsigned long long rogue_enemy_integration_replay_hash(int template_id, const int* unit_levels,
                                                           int unit_count, const int* modifier_ids,
                                                           int modifier_count);

    /* Record an encounter determinism snapshot (seed + hash) into a small ring buffer (last 32). */
    void rogue_enemy_integration_debug_record(unsigned int seed, unsigned long long hash,
                                              int template_id, int unit_count);

    /* Dump last N (<=32) encounter records to provided buffer as lines: idx seed hash template
     * unit_count. Returns bytes written. */
    int rogue_enemy_integration_debug_dump(char* buf, int buf_size);

    /* ================= Phase 2: Encounter Template → Room Placement =================
     * Functions to integrate encounter composition into dungeon room spawning.
     */

    /* Room metadata structure for integration */
    typedef struct RogueRoomEncounterInfo
    {
        int room_id;
        int depth_level;             /* room depth for difficulty calculation */
        int biome_id;                /* biome type for template weighting */
        int encounter_template_id;   /* selected template id (-1 if none) */
        unsigned int encounter_seed; /* computed seed for this encounter */
        int encounter_index;         /* index within room (usually 0) */
    } RogueRoomEncounterInfo;

    /* Choose encounter template id for a room based on depth, biome, and seed */
    int rogue_enemy_integration_choose_template(int room_depth, int biome_id, unsigned int seed,
                                                int* out_template_id);

    /* Compute difficulty rating for a room based on its depth and properties */
    int rogue_enemy_integration_compute_room_difficulty(int room_depth, int room_area,
                                                        int room_tags);

    /* Generate encounter info for a room (combines template selection and seed derivation) */
    int rogue_enemy_integration_prepare_room_encounter(const struct RogueDungeonRoom* room,
                                                       int world_seed, int region_id,
                                                       RogueRoomEncounterInfo* out_info);

    /* Validate that a template can be placed in a room (space requirements, etc.) */
    int rogue_enemy_integration_validate_template_placement(int template_id,
                                                            const struct RogueDungeonRoom* room);

    /* ================= Phase 3: Stat & Modifier Application at Spawn =================
     * Functions to apply final stats and modifiers to spawned enemies.
     */

    /* Apply stats and modifiers to a composed encounter unit */
    int rogue_enemy_integration_apply_unit_stats(struct RogueEnemy* enemy,
                                                 const struct RogueEncounterUnit* unit,
                                                 int player_level,
                                                 const RogueEnemyTypeMapping* type_mapping);

    /* Apply modifiers to an enemy based on encounter composition rules */
    int rogue_enemy_integration_apply_unit_modifiers(struct RogueEnemy* enemy,
                                                     const struct RogueEncounterUnit* unit,
                                                     unsigned int modifier_seed, int is_elite,
                                                     int is_boss);

    /* Complete spawn processing: stats + modifiers + encounter metadata */
    int rogue_enemy_integration_finalize_spawn(struct RogueEnemy* enemy,
                                               const struct RogueEncounterUnit* unit,
                                               const RogueRoomEncounterInfo* encounter_info,
                                               int player_level,
                                               const RogueEnemyTypeMapping* type_mapping);

    /* Validation: check stat non-negativity and budget adherence after modifier application */
    int rogue_enemy_integration_validate_final_stats(const struct RogueEnemy* enemy);

    /* ================= Phase 4: Visual & UI Exposure =================
     * Functions to support visual display of enemy information and modifiers.
     */

    /* Enemy nameplate/tooltip information */
    typedef struct RogueEnemyDisplayInfo
    {
        char name[64];                           /* display name */
        char tier_name[32];                      /* tier description (e.g., "Elite", "Boss") */
        int level;                               /* enemy level */
        int delta_level;                         /* ΔL relative to player */
        int is_elite;                            /* elite flag */
        int is_boss;                             /* boss flag */
        int is_support;                          /* support flag */
        int modifier_count;                      /* number of active modifiers */
        char modifier_tags[8][16];               /* short modifier tags for display */
        unsigned char color_r, color_g, color_b; /* nameplate color coding */
        RogueEnemyFinalStats final_stats;        /* computed stats for combat integration */
    } RogueEnemyDisplayInfo;

    /* Build display information for an enemy */
    int rogue_enemy_integration_build_display_info(const struct RogueEnemy* enemy, int player_level,
                                                   RogueEnemyDisplayInfo* out_info);

    /* Update HUD target feed when player target changes */
    int rogue_enemy_integration_update_hud_target(const struct RogueEnemy* target_enemy,
                                                  int player_level);

    /* Get color coding for enemy type (elite/boss/support) */
    void rogue_enemy_integration_get_type_color(const struct RogueEnemy* enemy,
                                                unsigned char* out_r, unsigned char* out_g,
                                                unsigned char* out_b);

    /* Get modifier telegraph token for visual effects */
    const char* rogue_enemy_integration_get_modifier_telegraph(int modifier_id);

    /* ================= Phase 5: Spatial Spawn & Navigation Integration ================= */
    typedef struct RogueSpawnSolution
    {
        float positions[16][2]; /* [x,y] positions for up to 16 enemies */
        int position_count;     /* Actual number of positions generated */
        int success;            /* 1 if all positions found, 0 if some failed */
        float room_bounds[4];   /* [min_x, min_y, max_x, max_y] */
        float min_distance;     /* Minimum distance between spawns */
    } RogueSpawnSolution;

    typedef struct RogueRoomDimensions
    {
        float min_x, min_y, max_x, max_y; /* Room bounds */
        float obstacle_zones[8][4];       /* Up to 8 obstacle rectangles [x1,y1,x2,y2] */
        int obstacle_count;               /* Number of obstacle zones */
    } RogueRoomDimensions;

    int rogue_enemy_integration_solve_spawn_positions(const RogueRoomEncounterInfo* encounter_info,
                                                      const RogueRoomDimensions* room_dims,
                                                      RogueSpawnSolution* out_solution);
    int rogue_enemy_integration_validate_spawn_position(
        float x, float y, const RogueRoomDimensions* room_dims,
        const RogueSpawnSolution* existing_positions, int check_count);
    int rogue_enemy_integration_register_navmesh_handles(const RogueSpawnSolution* solution,
                                                         const struct RogueEnemy* enemies,
                                                         int enemy_count);
    int rogue_enemy_integration_finalize_enemy_placement(const RogueSpawnSolution* solution,
                                                         struct RogueEnemy* enemies,
                                                         int enemy_count);

    /* ================= Phase 6: Target Acquisition & Combat Hook ================= */

#define MAX_REGISTERED_ENEMIES 256

    /* Enemy registry for target acquisition and HUD integration */
    typedef struct
    {
        int enemy_id;                       /* Unique runtime enemy ID */
        int room_id;                        /* Room containing this enemy */
        int encounter_id;                   /* Encounter that spawned this enemy */
        float position[2];                  /* World position for distance calculations */
        RogueEnemyDisplayInfo display_info; /* Cached display information */
        int is_alive;                       /* 1 if enemy is alive, 0 if dead */
    } RogueEnemyRegistryEntry;

    typedef struct
    {
        RogueEnemyRegistryEntry entries[MAX_REGISTERED_ENEMIES];
        int count;
        int next_enemy_id;
    } RogueEnemyRegistry;

    /* Find nearest enemy to given position within max_distance */
    int rogue_enemy_integration_find_nearest_enemy(const RogueEnemyRegistry* registry,
                                                   float position[2], float max_distance,
                                                   int* out_enemy_id);

    /* Find enemy at screen/world position for hover/click targeting */
    int rogue_enemy_integration_find_enemy_at_position(const RogueEnemyRegistry* registry,
                                                       float position[2], float tolerance,
                                                       int* out_enemy_id);

    /* Register enemy for targeting and combat integration */
    int rogue_enemy_integration_register_enemy(RogueEnemyRegistry* registry, int room_id,
                                               int encounter_id, float position[2],
                                               const RogueEnemyDisplayInfo* display_info);

    /* Update enemy position in registry */
    void rogue_enemy_integration_update_enemy_position(RogueEnemyRegistry* registry, int enemy_id,
                                                       float position[2]);

    /* Get enemy display info for HUD ΔL indicator */
    int rogue_enemy_integration_get_enemy_display_info(const RogueEnemyRegistry* registry,
                                                       int enemy_id,
                                                       RogueEnemyDisplayInfo* out_display_info);

    /* Mark enemy as dead and emit analytics event */
    void rogue_enemy_integration_mark_enemy_dead(RogueEnemyRegistry* registry, int enemy_id);

    /* Combat integration: get enemy final stats for damage calculation */
    typedef struct
    {
        float max_health;
        float current_health;
        float base_damage;
        float armor_rating;
        float crit_chance;
        float crit_multiplier;
        float fire_resist;
        float cold_resist;
        float lightning_resist;
        float poison_resist;
    } RogueEnemyCombatStats;

    int rogue_enemy_integration_get_combat_stats(const RogueEnemyRegistry* registry, int enemy_id,
                                                 RogueEnemyCombatStats* out_stats);

    /* Combat event: apply damage and check for death */
    int rogue_enemy_integration_apply_damage(
        RogueEnemyRegistry* registry, int enemy_id, float damage,
        int damage_type); /* 0=physical, 1=fire, 2=cold, 3=lightning, 4=poison */

    /* Clean up dead enemies from registry */
    void rogue_enemy_integration_cleanup_dead_enemies(RogueEnemyRegistry* registry);

#ifdef __cplusplus
}
#endif
#endif
