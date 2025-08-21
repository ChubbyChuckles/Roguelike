#ifndef ROGUE_COMBAT_PROGRESSION_BRIDGE_H
#define ROGUE_COMBAT_PROGRESSION_BRIDGE_H

#include "event_bus.h"
#include <stdint.h>
#include <stdbool.h>

/* Phase 3.3 Combat System ↔ Character Progression Bridge */

/* Combat-Progression Bridge Event Types */
#define ROGUE_COMBAT_PROG_EVENT_XP_AWARDED           0x3301
#define ROGUE_COMBAT_PROG_EVENT_SKILL_USED          0x3302
#define ROGUE_COMBAT_PROG_EVENT_ACHIEVEMENT_UNLOCKED 0x3303
#define ROGUE_COMBAT_PROG_EVENT_PLAYSTYLE_DETECTED   0x3304
#define ROGUE_COMBAT_PROG_EVENT_EFFICIENCY_MILESTONE 0x3305
#define ROGUE_COMBAT_PROG_EVENT_DEATH_PENALTY_APPLIED 0x3306
#define ROGUE_COMBAT_PROG_EVENT_PASSIVE_EFFECT_APPLIED 0x3307

/* Combat XP Calculation Type (Phase 3.3.1) */
typedef enum {
    ROGUE_XP_SOURCE_DAMAGE_DEALT,
    ROGUE_XP_SOURCE_ENEMY_DEFEATED,
    ROGUE_XP_SOURCE_DIFFICULTY_BONUS,
    ROGUE_XP_SOURCE_EFFICIENCY_BONUS,
    ROGUE_XP_SOURCE_COUNT
} RogueCombatXpSource;

/* Combat XP Distribution Event (Phase 3.3.1) */
typedef struct {
    uint32_t combat_event_id;
    RogueCombatXpSource xp_source;
    uint32_t damage_dealt;
    uint32_t enemy_difficulty;
    uint32_t base_xp;
    uint32_t bonus_xp;
    uint32_t total_xp_awarded;
    float difficulty_multiplier;
} RogueCombatXpDistribution;

/* Combat Skill Usage Event (Phase 3.3.2) */
typedef struct {
    uint16_t skill_id;
    uint16_t skill_level;
    uint32_t usage_count;
    uint32_t effectiveness_score;
    uint32_t mastery_points_gained;
    uint32_t combat_context_id;
    bool mastery_threshold_reached;
} RogueCombatSkillUsage;

/* Passive Skill Effect (Phase 3.3.3) */
typedef struct {
    uint16_t passive_skill_id;
    uint8_t effect_type;          /* 0=damage mod, 1=defense mod, 2=speed mod, etc */
    float effect_magnitude;       /* Multiplier or flat bonus */
    uint32_t activation_condition; /* Bitmask of conditions */
    bool is_active;
    uint32_t duration_ms;         /* 0 = permanent */
} RoguePassiveSkillEffect;

/* Combat Achievement (Phase 3.3.4) */
typedef struct {
    uint32_t achievement_id;
    char achievement_name[64];
    uint32_t trigger_condition;   /* What combat event triggered it */
    uint32_t progress_current;
    uint32_t progress_required;
    bool just_unlocked;
    uint32_t reward_xp;
} RogueCombatAchievement;

/* Combat Playstyle Analysis (Phase 3.3.5) */
typedef enum {
    ROGUE_PLAYSTYLE_AGGRESSIVE,
    ROGUE_PLAYSTYLE_DEFENSIVE,
    ROGUE_PLAYSTYLE_BALANCED,
    ROGUE_PLAYSTYLE_TACTICAL,
    ROGUE_PLAYSTYLE_MAGICAL,
    ROGUE_PLAYSTYLE_COUNT
} RogueCombatPlaystyle;

typedef struct {
    RogueCombatPlaystyle detected_style;
    float confidence_score;       /* 0.0 to 1.0 */
    uint32_t combat_sessions_analyzed;
    uint32_t damage_preference;   /* Physical vs magical preference */
    uint32_t risk_tolerance;      /* Low health combat frequency */
    uint32_t tactical_usage;      /* Special abilities/positioning */
} RogueCombatPlaystyleProfile;

/* Combat Efficiency Metrics (Phase 3.3.6) */
typedef struct {
    float damage_per_second_avg;
    float damage_per_mana_efficiency;
    float time_to_kill_avg_ms;
    float resource_usage_efficiency; /* HP/MP conservation */
    uint32_t combat_streak_count;
    uint32_t perfect_combat_count;   /* No damage taken */
    float overall_efficiency_score;  /* Composite 0-100 */
} RogueCombatEfficiencyMetrics;

/* Combat Death Penalty (Phase 3.3.7) */
typedef struct {
    uint32_t death_count;
    uint32_t xp_penalty_amount;
    float xp_penalty_percentage;
    uint32_t equipment_durability_loss;
    uint32_t currency_penalty;
    uint32_t skill_penalty_duration_ms;
    bool resurrection_items_consumed;
} RogueCombatDeathPenalty;

/* Main Combat-Progression Bridge Structure */
typedef struct {
    bool initialized;
    uint64_t last_update_timestamp;
    
    /* Phase 3.3.1: XP Distribution System */
    RogueCombatXpDistribution xp_distributions[32];
    uint8_t xp_distribution_count;
    uint32_t total_xp_awarded_session;
    float current_difficulty_multiplier;
    
    /* Phase 3.3.2: Skill Usage Tracking */
    RogueCombatSkillUsage skill_usage_tracking[16];
    uint8_t tracked_skills_count;
    uint32_t total_skill_activations;
    
    /* Phase 3.3.3: Passive Skills */
    RoguePassiveSkillEffect active_passive_effects[24];
    uint8_t active_passives_count;
    bool passive_effects_dirty;
    
    /* Phase 3.3.4: Achievement System */
    RogueCombatAchievement achievements[64];
    uint8_t achievement_count;
    uint32_t achievements_unlocked_session;
    
    /* Phase 3.3.5: Playstyle Analysis */
    RogueCombatPlaystyleProfile playstyle_profile;
    uint32_t playstyle_data_points[8];   /* Raw metrics for analysis */
    bool playstyle_analysis_dirty;
    
    /* Phase 3.3.6: Efficiency Metrics */
    RogueCombatEfficiencyMetrics efficiency_metrics;
    uint32_t efficiency_milestone_count;
    
    /* Phase 3.3.7: Death Penalty System */
    RogueCombatDeathPenalty death_penalty_state;
    
    /* Performance & Debug */
    struct {
        uint32_t xp_distributions_processed;
        uint32_t skill_usage_events_processed;
        uint32_t passive_effects_applied;
        uint32_t achievements_triggered;
        uint32_t playstyle_analyses_performed;
        uint32_t efficiency_calculations_performed;
        uint32_t death_penalties_applied;
        float avg_processing_time_ms;
        uint64_t total_processing_time_us;
    } metrics;
    
    /* Debug & Logging */
    bool debug_mode;
    char last_error[128];
    uint32_t error_count;
    
} RogueCombatProgressionBridge;

/* Phase 3.3.1: Combat XP Distribution Functions */
int rogue_combat_progression_bridge_init(RogueCombatProgressionBridge* bridge);
void rogue_combat_progression_bridge_shutdown(RogueCombatProgressionBridge* bridge);
int rogue_combat_progression_bridge_update(RogueCombatProgressionBridge* bridge, float dt_ms);

int rogue_combat_progression_bridge_award_xp(RogueCombatProgressionBridge* bridge,
                                              RogueCombatXpSource source,
                                              uint32_t damage_dealt,
                                              uint32_t enemy_difficulty,
                                              uint32_t combat_event_id);

/* Phase 3.3.2: Skill Usage Tracking Functions */
int rogue_combat_progression_bridge_track_skill_usage(RogueCombatProgressionBridge* bridge,
                                                       uint16_t skill_id,
                                                       uint32_t effectiveness_score,
                                                       uint32_t combat_context_id);

int rogue_combat_progression_bridge_get_skill_mastery_progress(RogueCombatProgressionBridge* bridge,
                                                                uint16_t skill_id,
                                                                uint32_t* out_progress,
                                                                uint32_t* out_required);

/* Phase 3.3.3: Passive Skill Effects Functions */
int rogue_combat_progression_bridge_apply_passive_effects(RogueCombatProgressionBridge* bridge,
                                                           float* damage_modifier,
                                                           float* defense_modifier,
                                                           float* speed_modifier);

int rogue_combat_progression_bridge_activate_passive_skill(RogueCombatProgressionBridge* bridge,
                                                            uint16_t skill_id,
                                                            uint8_t effect_type,
                                                            float magnitude,
                                                            uint32_t duration_ms);

/* Phase 3.3.4: Achievement System Functions */
int rogue_combat_progression_bridge_check_achievements(RogueCombatProgressionBridge* bridge,
                                                        uint32_t combat_event_type,
                                                        uint32_t event_data);

int rogue_combat_progression_bridge_get_recent_achievements(RogueCombatProgressionBridge* bridge,
                                                             RogueCombatAchievement* out_achievements,
                                                             uint8_t max_count);

/* Phase 3.3.5: Playstyle Analysis Functions */
int rogue_combat_progression_bridge_analyze_playstyle(RogueCombatProgressionBridge* bridge,
                                                       uint32_t damage_type_preference,
                                                       uint32_t risk_behavior,
                                                       uint32_t tactical_complexity);

RogueCombatPlaystyle rogue_combat_progression_bridge_get_detected_playstyle(RogueCombatProgressionBridge* bridge,
                                                                             float* out_confidence);

/* Phase 3.3.6: Efficiency Metrics Functions */
int rogue_combat_progression_bridge_update_efficiency_metrics(RogueCombatProgressionBridge* bridge,
                                                               float combat_duration_ms,
                                                               uint32_t damage_dealt,
                                                               uint32_t mana_used,
                                                               uint32_t damage_taken);

int rogue_combat_progression_bridge_get_efficiency_score(RogueCombatProgressionBridge* bridge,
                                                          float* out_overall_score,
                                                          RogueCombatEfficiencyMetrics* out_detailed);

/* Phase 3.3.7: Death Penalty Functions */
int rogue_combat_progression_bridge_apply_death_penalty(RogueCombatProgressionBridge* bridge,
                                                         uint32_t player_level,
                                                         uint32_t current_xp);

int rogue_combat_progression_bridge_get_death_penalty_info(RogueCombatProgressionBridge* bridge,
                                                            RogueCombatDeathPenalty* out_penalty_info);

/* Debug & Utility Functions */
void rogue_combat_progression_bridge_set_debug_mode(RogueCombatProgressionBridge* bridge, bool enabled);
void rogue_combat_progression_bridge_get_metrics(RogueCombatProgressionBridge* bridge, char* out_buffer, size_t buffer_size);
void rogue_combat_progression_bridge_reset_metrics(RogueCombatProgressionBridge* bridge);

#endif /* ROGUE_COMBAT_PROGRESSION_BRIDGE_H */
