/**
 * @file enemy.h
 * @brief Enemy entity system with AI, combat stats, and behavior trees.
 *
 * Defines the comprehensive enemy system including enemy type definitions,
 * individual enemy instances, AI states, combat statistics, status effects,
 * and behavior tree integration. Supports complex enemy mechanics like
 * difficulty scaling, encounter modifiers, AI intensity systems, and
 * advanced combat features.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

#ifndef ROGUE_ENTITIES_ENEMY_H
#define ROGUE_ENTITIES_ENEMY_H
#include "../graphics/sprite.h"
#include "entity.h"

/**
 * @brief AI state enumeration for enemy behavior.
 *
 * Defines the basic AI states that enemies can be in, controlling
 * their behavior patterns and decision making.
 */
typedef enum RogueEnemyAIState
{
    ROGUE_ENEMY_AI_PATROL = 0, ///< Patrolling/idle state
    ROGUE_ENEMY_AI_AGGRO,      ///< Aggressive/combat state  
    ROGUE_ENEMY_AI_DEAD        ///< Dead state
} RogueEnemyAIState;

/**
 * @brief Enemy type definition structure.
 *
 * Defines the template/blueprint for a specific enemy type including
 * base stats, behavior parameters, loot configuration, and animation
 * resources. Multiple enemy instances can share the same type definition.
 */
typedef struct RogueEnemyTypeDef
{
    char id[32];               ///< Unique identifier for JSON loading
    char name[32];             ///< Display name for UI
    int weight;                ///< Spawn weighting for encounter generation
    int group_min;             ///< Minimum group size when spawning
    int group_max;             ///< Maximum group size when spawning
    int patrol_radius;         ///< Patrol radius in tiles
    int aggro_radius;          ///< Aggression detection radius in tiles
    float speed;               ///< Movement speed in tiles per second
    int pop_target;            ///< Desired population count for this type
    int xp_reward;             ///< Experience points awarded per kill
    float loot_chance;         ///< Probability of dropping loot (0.0-1.0)
    int base_level_offset;     ///< Level offset relative to area/player level
    int tier_id;               ///< Difficulty tier classification
    int archetype_id;          ///< Behavioral archetype (melee/ranged/caster/etc.)
    
    /* Animation resources */
    RogueTexture idle_tex;     ///< Texture for idle animation
    RogueTexture run_tex;      ///< Texture for running animation
    RogueTexture death_tex;    ///< Texture for death animation
    RogueSprite idle_frames[8]; ///< Idle animation frame sprites
    int idle_count;            ///< Number of idle animation frames
    RogueSprite run_frames[8]; ///< Run animation frame sprites
    int run_count;             ///< Number of run animation frames
    RogueSprite death_frames[8]; ///< Death animation frame sprites
    int death_count;           ///< Number of death animation frames
} RogueEnemyTypeDef;

/**
 * @brief Individual enemy instance structure.
 *
 * Represents a single enemy entity with all runtime state including
 * health, AI state, combat stats, status effects, modifiers, and
 * behavior tree integration. Contains both base properties and
 * advanced systems like intensity scaling and elite/boss flags.
 */
typedef struct RogueEnemy
{
    RogueEntity base;          ///< Base entity properties (position, movement, etc.)
    unsigned char team_id;     ///< Team/faction ID (e.g., 1=enemies)
    int type_index;            ///< Index into enemy type definition array
    
    /* Health and status */
    int health;                ///< Current health points
    int max_health;            ///< Maximum health (persistent for health bar ratios)
    int level;                 ///< Enemy level (affects knockback and scaling)
    int alive;                 ///< 1 if alive, 0 if removed/dead
    float hurt_timer;          ///< Remaining hurt flash duration in milliseconds
    
    /* Animation state */
    float anim_time;           ///< Current animation time in milliseconds
    int anim_frame;            ///< Current animation frame index
    
    /* AI and movement */
    RogueEnemyAIState ai_state;        ///< Current AI behavioral state
    float anchor_x, anchor_y;          ///< Group formation anchor point
    float patrol_target_x, patrol_target_y; ///< Current patrol destination
    int facing;                        ///< Facing direction (1=left, 2=right for sprite flipping)
    
    /* Visual effects */
    float tint_r, tint_g, tint_b;      ///< Color tint modulation (0-255 range stored as floats)
    float death_fade;                  ///< Death fade alpha (1.0 -> 0.0)
    float tint_phase;                  ///< Time accumulator for pulsing tint effects
    float flash_timer;                 ///< Brief hit flash effect timer in milliseconds
    
    /* Combat stats */
    float attack_cooldown_ms;          ///< Time until next attack is allowed
    int crit_chance;                   ///< Critical hit chance percentage
    int crit_damage;                   ///< Critical damage bonus percentage over 100
    
    /* Status effect buildup */
    float bleed_buildup;               ///< Bleed effect accumulation (future feature)
    float frost_buildup;               ///< Frost effect accumulation (future slow/freeze)
    
    /* Damage mitigation stats */
    int armor;                         ///< Physical damage mitigation (flat reduction)
    int resist_physical;               ///< Physical damage resistance percentage (0-90)
    int resist_fire;                   ///< Fire damage resistance percentage (0-90)
    int resist_frost;                  ///< Frost damage resistance percentage (0-90)
    int resist_arcane;                 ///< Arcane damage resistance percentage (0-90)
    int resist_bleed;                  ///< Bleed damage resistance percentage
    int resist_poison;                 ///< Poison damage resistance percentage
    
    /* Guard and poise system */
    float guard_meter;                 ///< Current guard meter (for future blocking AI)
    float guard_meter_max;             ///< Maximum guard meter capacity
    float poise;                       ///< Current poise (stability against stagger)
    float poise_max;                   ///< Maximum poise capacity
    int staggered;                     ///< 1 if currently staggered
    float stagger_timer_ms;            ///< Remaining stagger duration
    
    /* AI Behavior Tree integration */
    unsigned char ai_bt_enabled;       ///< 1 if using behavior tree instead of legacy AI
    struct RogueBehaviorTree* ai_tree; ///< Behavior tree root node (owned, destroyed on despawn)
    void* ai_bt_state;                 ///< Blackboard/state wrapper for behavior tree
    
    /* Encounter and difficulty system */
    int tier_id;                       ///< Difficulty tier classification
    int base_level_offset;             ///< Level offset cached from type definition
    int encounter_id;                  ///< Encounter instance ID for analytics/replay
    unsigned int replay_hash_fragment; ///< Partial hash for encounter composition
    
    /* Special enemy flags */
    unsigned char elite_flag;          ///< 1 if this is an elite enemy variant
    unsigned char boss_flag;           ///< 1 if this is a boss enemy
    unsigned char support_flag;        ///< 1 if this is a support/utility enemy
    unsigned char modifier_count;      ///< Number of active modifiers
    int modifier_ids[8];               ///< Array of applied modifier IDs (max 8)
    
    /* Cached combat stats */
    float final_hp;                    ///< Final calculated health after modifiers
    float final_damage;                ///< Final calculated damage after modifiers
    float final_defense;               ///< Final calculated defense after modifiers
    
    /* AI intensity system */
    unsigned char ai_intensity;        ///< Current AI intensity level (enum value)
    float ai_intensity_score;          ///< Internal escalation score for intensity
    float ai_intensity_cooldown_ms;    ///< Cooldown preventing rapid intensity changes
} RogueEnemy;

/**
 * @brief Maximum number of enemies that can exist simultaneously.
 */
#define ROGUE_MAX_ENEMIES 256

/**
 * @brief Maximum number of different enemy types that can be defined.
 */
#define ROGUE_MAX_ENEMY_TYPES 16

/**
 * @brief Load enemy type definitions from a configuration file.
 *
 * Loads enemy type definitions from a configuration file, populating
 * the provided type array. The input/output count parameter specifies
 * the current count and receives the new count after loading.
 *
 * @param path Path to the configuration file
 * @param types Array to store loaded enemy type definitions
 * @param inout_type_count Pointer to current count (input) and new count (output)
 * @return Non-zero on success, 0 on failure
 */
int rogue_enemy_load_config(const char* path, RogueEnemyTypeDef types[], int* inout_type_count);

/**
 * @brief Load enemy types from JSON files in a directory.
 *
 * Scans a directory for JSON files containing enemy type definitions
 * and loads them into the provided array. Modern replacement for
 * legacy configuration file loading.
 *
 * @param dir_path Path to directory containing JSON enemy definitions
 * @param types Array to store loaded enemy type definitions
 * @param max_types Maximum number of types the array can hold
 * @param out_count Pointer to receive the number of types loaded
 * @return Non-zero on success, 0 on failure
 */
int rogue_enemy_types_load_directory_json(const char* dir_path, RogueEnemyTypeDef types[],
                                          int max_types, int* out_count);

/**
 * @brief Configure texture loading for enemy types.
 *
 * Allows tests and headless runs to skip texture loading for enemy
 * type definitions, improving performance when graphics are not needed.
 *
 * @param skip 1 to skip texture loading, 0 to load textures normally
 */
void rogue_enemy_loader_set_skip_textures(int skip);

/**
 * @brief Enable behavior tree AI for an enemy.
 *
 * Switches the enemy from legacy AI to behavior tree-based AI system.
 * Initializes the behavior tree and associated state structures.
 *
 * @param e Pointer to the enemy to enable behavior tree AI for
 */
void rogue_enemy_ai_bt_enable(struct RogueEnemy* e);

/**
 * @brief Disable behavior tree AI for an enemy.
 *
 * Switches the enemy back to legacy AI and cleans up behavior tree
 * resources. The behavior tree and state are properly destroyed.
 *
 * @param e Pointer to the enemy to disable behavior tree AI for
 */
void rogue_enemy_ai_bt_disable(struct RogueEnemy* e);

/**
 * @brief Execute one behavior tree update tick.
 *
 * Runs the behavior tree AI for one frame, updating the enemy's
 * decision making and actions based on the current game state.
 * Only functional when behavior tree AI is enabled for the enemy.
 *
 * @param e Pointer to the enemy to update
 * @param dt_seconds Delta time in seconds since last update
 */
void rogue_enemy_ai_bt_tick(struct RogueEnemy* e, float dt_seconds);

#endif
