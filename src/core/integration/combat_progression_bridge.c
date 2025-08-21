#include "combat_progression_bridge.h"
#include "event_bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#define BRIDGE_LOG(bridge, level, fmt, ...) do { \
    if ((bridge)->debug_mode) { \
        printf("[COMBAT-PROG-BRIDGE][%s] " fmt "\n", level, ##__VA_ARGS__); \
    } \
} while(0)
#else
#define BRIDGE_LOG(bridge, level, fmt, ...) do { \
    if ((bridge)->debug_mode) { \
        printf("[COMBAT-PROG-BRIDGE][%s] " fmt "\n", level, ##__VA_ARGS__); \
    } \
} while(0)
#endif

/* Phase 3.3.1: Combat XP Distribution Implementation */
int rogue_combat_progression_bridge_init(RogueCombatProgressionBridge* bridge) {
    if (!bridge) {
        return 0;
    }
    
    memset(bridge, 0, sizeof(RogueCombatProgressionBridge));
    
    /* Initialize default values */
    bridge->current_difficulty_multiplier = 1.0f;
    bridge->playstyle_profile.detected_style = ROGUE_PLAYSTYLE_BALANCED;
    bridge->playstyle_profile.confidence_score = 0.0f;
    bridge->efficiency_metrics.overall_efficiency_score = 50.0f; /* Baseline */
    
    bridge->initialized = true;
    bridge->last_update_timestamp = (uint64_t)time(NULL);
    bridge->debug_mode = true; /* Enable debug by default */
    
    BRIDGE_LOG(bridge, "INFO", "Combat-Progression Bridge initialized successfully");
    
    return 1;
}

void rogue_combat_progression_bridge_shutdown(RogueCombatProgressionBridge* bridge) {
    if (!bridge || !bridge->initialized) {
        return;
    }
    
    BRIDGE_LOG(bridge, "INFO", "Shutting down Combat-Progression Bridge. Stats: XP events=%u, Skills tracked=%u, Achievements=%u",
               bridge->metrics.xp_distributions_processed,
               bridge->metrics.skill_usage_events_processed,
               bridge->metrics.achievements_triggered);
    
    memset(bridge, 0, sizeof(RogueCombatProgressionBridge));
}

int rogue_combat_progression_bridge_update(RogueCombatProgressionBridge* bridge, float dt_ms) {
    if (!bridge || !bridge->initialized) {
        return 0;
    }
    
    /* Update passive skill effect timers */
    uint8_t active_count = 0;
    for (uint8_t i = 0; i < bridge->active_passives_count; i++) {
        RoguePassiveSkillEffect* passive = &bridge->active_passive_effects[i];
        
        if (passive->duration_ms > 0) {
            if (passive->duration_ms > (uint32_t)dt_ms) {
                passive->duration_ms -= (uint32_t)dt_ms;
                bridge->active_passive_effects[active_count] = *passive;
                active_count++;
            } else {
                /* Passive effect expired */
                BRIDGE_LOG(bridge, "DEBUG", "Passive skill %u expired", passive->passive_skill_id);
            }
        } else {
            /* Permanent passive */
            bridge->active_passive_effects[active_count] = *passive;
            active_count++;
        }
    }
    
    if (active_count != bridge->active_passives_count) {
        bridge->active_passives_count = active_count;
        bridge->passive_effects_dirty = true;
    }
    
    bridge->last_update_timestamp = (uint64_t)time(NULL);
    
    return 1;
}

int rogue_combat_progression_bridge_award_xp(RogueCombatProgressionBridge* bridge,
                                              RogueCombatXpSource source,
                                              uint32_t damage_dealt,
                                              uint32_t enemy_difficulty,
                                              uint32_t combat_event_id) {
    if (!bridge || !bridge->initialized || bridge->xp_distribution_count >= 32) {
        return 0;
    }
    
#ifdef _WIN32
    LARGE_INTEGER start, frequency;
    QueryPerformanceCounter(&start);
    QueryPerformanceFrequency(&frequency);
#endif
    
    RogueCombatXpDistribution* xp_dist = &bridge->xp_distributions[bridge->xp_distribution_count];
    
    /* Calculate base XP based on damage and difficulty */
    uint32_t base_xp = (damage_dealt / 10) + (enemy_difficulty * 5);
    
    /* Apply difficulty multiplier */
    float difficulty_multiplier = bridge->current_difficulty_multiplier;
    if (enemy_difficulty > 50) {
        difficulty_multiplier += 0.1f * ((enemy_difficulty - 50) / 10.0f);
    }
    
    /* Calculate bonus XP based on source type */
    uint32_t bonus_xp = 0;
    switch (source) {
        case ROGUE_XP_SOURCE_DAMAGE_DEALT:
            bonus_xp = damage_dealt / 20;
            break;
        case ROGUE_XP_SOURCE_ENEMY_DEFEATED:
            bonus_xp = enemy_difficulty * 2;
            break;
        case ROGUE_XP_SOURCE_DIFFICULTY_BONUS:
            bonus_xp = (uint32_t)(base_xp * 0.5f);
            break;
        case ROGUE_XP_SOURCE_EFFICIENCY_BONUS:
            bonus_xp = (uint32_t)(bridge->efficiency_metrics.overall_efficiency_score * 0.1f);
            break;
        default:
            break;
    }
    
    uint32_t total_xp = (uint32_t)((base_xp + bonus_xp) * difficulty_multiplier);
    
    /* Fill out the XP distribution record */
    xp_dist->combat_event_id = combat_event_id;
    xp_dist->xp_source = source;
    xp_dist->damage_dealt = damage_dealt;
    xp_dist->enemy_difficulty = enemy_difficulty;
    xp_dist->base_xp = base_xp;
    xp_dist->bonus_xp = bonus_xp;
    xp_dist->total_xp_awarded = total_xp;
    xp_dist->difficulty_multiplier = difficulty_multiplier;
    
    bridge->xp_distribution_count++;
    bridge->total_xp_awarded_session += total_xp;
    bridge->metrics.xp_distributions_processed++;
    
    /* Publish XP award event */
    RogueEventPayload payload = {0};
    memcpy(payload.raw_data, &total_xp, sizeof(uint32_t));
    rogue_event_publish(ROGUE_COMBAT_PROG_EVENT_XP_AWARDED,
                        &payload, ROGUE_EVENT_PRIORITY_NORMAL,
                        0, "CombatProgressionBridge");
    
#ifdef _WIN32
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);
    double time_taken = (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart * 1000000.0;
    bridge->metrics.total_processing_time_us += (uint64_t)time_taken;
    bridge->metrics.avg_processing_time_ms = (float)bridge->metrics.total_processing_time_us / 
                                             bridge->metrics.xp_distributions_processed / 1000.0f;
#endif
    
    BRIDGE_LOG(bridge, "INFO", "Awarded %u XP (base=%u, bonus=%u, mult=%.2f) from %s for combat event %u", 
               total_xp, base_xp, bonus_xp, difficulty_multiplier, 
               (source == ROGUE_XP_SOURCE_DAMAGE_DEALT) ? "damage" : 
               (source == ROGUE_XP_SOURCE_ENEMY_DEFEATED) ? "defeat" : 
               (source == ROGUE_XP_SOURCE_DIFFICULTY_BONUS) ? "difficulty" : "efficiency",
               combat_event_id);
    
    return 1;
}

/* Phase 3.3.2: Skill Usage Tracking Implementation */
int rogue_combat_progression_bridge_track_skill_usage(RogueCombatProgressionBridge* bridge,
                                                       uint16_t skill_id,
                                                       uint32_t effectiveness_score,
                                                       uint32_t combat_context_id) {
    if (!bridge || !bridge->initialized) {
        return 0;
    }
    
    /* Find existing skill tracking entry or create new one */
    RogueCombatSkillUsage* skill_usage = NULL;
    for (uint8_t i = 0; i < bridge->tracked_skills_count; i++) {
        if (bridge->skill_usage_tracking[i].skill_id == skill_id) {
            skill_usage = &bridge->skill_usage_tracking[i];
            break;
        }
    }
    
    /* Create new entry if not found */
    if (!skill_usage && bridge->tracked_skills_count < 16) {
        skill_usage = &bridge->skill_usage_tracking[bridge->tracked_skills_count];
        skill_usage->skill_id = skill_id;
        skill_usage->skill_level = 1; /* TODO: Get actual skill level */
        skill_usage->usage_count = 0;
        skill_usage->effectiveness_score = 0;
        skill_usage->mastery_points_gained = 0;
        bridge->tracked_skills_count++;
    }
    
    if (!skill_usage) {
        return 0; /* No space for new skills */
    }
    
    /* Update skill usage statistics */
    skill_usage->usage_count++;
    skill_usage->effectiveness_score = (skill_usage->effectiveness_score + effectiveness_score) / 2;
    skill_usage->combat_context_id = combat_context_id;
    
    /* Calculate mastery points based on effectiveness */
    uint32_t mastery_gained = effectiveness_score / 10 + 1;
    skill_usage->mastery_points_gained += mastery_gained;
    
    /* Check for mastery threshold (arbitrary threshold of 1000 points) */
    uint32_t mastery_threshold = 1000 * skill_usage->skill_level;
    skill_usage->mastery_threshold_reached = (skill_usage->mastery_points_gained >= mastery_threshold);
    
    if (skill_usage->mastery_threshold_reached) {
        skill_usage->skill_level++;
        skill_usage->mastery_points_gained = 0; /* Reset for next level */
        
        BRIDGE_LOG(bridge, "INFO", "Skill %u reached mastery level %u!", 
                   skill_id, skill_usage->skill_level);
    }
    
    bridge->total_skill_activations++;
    bridge->metrics.skill_usage_events_processed++;
    
    /* Publish skill usage event */
    RogueEventPayload payload = {0};
    memcpy(payload.raw_data, &skill_id, sizeof(uint16_t));
    rogue_event_publish(ROGUE_COMBAT_PROG_EVENT_SKILL_USED,
                        &payload, ROGUE_EVENT_PRIORITY_NORMAL,
                        0, "CombatProgressionBridge");
    
    BRIDGE_LOG(bridge, "DEBUG", "Tracked skill %u usage: count=%u, effectiveness=%u, mastery=%u",
               skill_id, skill_usage->usage_count, effectiveness_score, mastery_gained);
    
    return 1;
}

int rogue_combat_progression_bridge_get_skill_mastery_progress(RogueCombatProgressionBridge* bridge,
                                                                uint16_t skill_id,
                                                                uint32_t* out_progress,
                                                                uint32_t* out_required) {
    if (!bridge || !bridge->initialized || !out_progress || !out_required) {
        return 0;
    }
    
    /* Find skill in tracking array */
    for (uint8_t i = 0; i < bridge->tracked_skills_count; i++) {
        if (bridge->skill_usage_tracking[i].skill_id == skill_id) {
            RogueCombatSkillUsage* skill = &bridge->skill_usage_tracking[i];
            *out_progress = skill->mastery_points_gained;
            *out_required = 1000 * skill->skill_level;
            return 1;
        }
    }
    
    /* Skill not tracked yet */
    *out_progress = 0;
    *out_required = 1000;
    return 0;
}

/* Phase 3.3.3: Passive Skill Effects Implementation */
int rogue_combat_progression_bridge_apply_passive_effects(RogueCombatProgressionBridge* bridge,
                                                           float* damage_modifier,
                                                           float* defense_modifier,
                                                           float* speed_modifier) {
    if (!bridge || !bridge->initialized || !damage_modifier || !defense_modifier || !speed_modifier) {
        return 0;
    }
    
    /* Initialize base modifiers */
    *damage_modifier = 1.0f;
    *defense_modifier = 1.0f;
    *speed_modifier = 1.0f;
    
    uint32_t effects_applied = 0;
    
    /* Apply all active passive effects */
    for (uint8_t i = 0; i < bridge->active_passives_count; i++) {
        RoguePassiveSkillEffect* passive = &bridge->active_passive_effects[i];
        
        if (!passive->is_active) {
            continue;
        }
        
        switch (passive->effect_type) {
            case 0: /* Damage modifier */
                *damage_modifier *= passive->effect_magnitude;
                effects_applied++;
                break;
            case 1: /* Defense modifier */
                *defense_modifier *= passive->effect_magnitude;
                effects_applied++;
                break;
            case 2: /* Speed modifier */
                *speed_modifier *= passive->effect_magnitude;
                effects_applied++;
                break;
            default:
                /* Unknown effect type */
                break;
        }
    }
    
    bridge->metrics.passive_effects_applied += effects_applied;
    
    if (effects_applied > 0) {
        BRIDGE_LOG(bridge, "DEBUG", "Applied %u passive effects: dmg=%.2f, def=%.2f, speed=%.2f",
                   effects_applied, *damage_modifier, *defense_modifier, *speed_modifier);
    }
    
    return (int)effects_applied;
}

int rogue_combat_progression_bridge_activate_passive_skill(RogueCombatProgressionBridge* bridge,
                                                            uint16_t skill_id,
                                                            uint8_t effect_type,
                                                            float magnitude,
                                                            uint32_t duration_ms) {
    if (!bridge || !bridge->initialized || bridge->active_passives_count >= 24) {
        return 0;
    }
    
    RoguePassiveSkillEffect* passive = &bridge->active_passive_effects[bridge->active_passives_count];
    
    passive->passive_skill_id = skill_id;
    passive->effect_type = effect_type;
    passive->effect_magnitude = magnitude;
    passive->activation_condition = 0; /* Always active for now */
    passive->is_active = true;
    passive->duration_ms = duration_ms;
    
    bridge->active_passives_count++;
    bridge->passive_effects_dirty = true;
    
    /* Publish passive effect event */
    RogueEventPayload payload = {0};
    memcpy(payload.raw_data, &skill_id, sizeof(uint16_t));
    rogue_event_publish(ROGUE_COMBAT_PROG_EVENT_PASSIVE_EFFECT_APPLIED,
                        &payload, ROGUE_EVENT_PRIORITY_NORMAL,
                        0, "CombatProgressionBridge");
    
    BRIDGE_LOG(bridge, "INFO", "Activated passive skill %u: type=%u, magnitude=%.2f, duration=%ums",
               skill_id, effect_type, magnitude, duration_ms);
    
    return 1;
}

/* Phase 3.3.4: Achievement System Implementation */
int rogue_combat_progression_bridge_check_achievements(RogueCombatProgressionBridge* bridge,
                                                        uint32_t combat_event_type,
                                                        uint32_t event_data) {
    if (!bridge || !bridge->initialized) {
        return 0;
    }
    
    uint32_t achievements_triggered = 0;
    
    /* Example achievements (would be loaded from configuration in real system) */
    struct {
        uint32_t id;
        char name[64];
        uint32_t trigger_type;
        uint32_t required_value;
    } achievement_definitions[] = {
        {1001, "First Strike", 0x01, 1},        /* First enemy killed */
        {1002, "Damage Dealer", 0x02, 1000},   /* 1000 damage dealt */
        {1003, "XP Hunter", 0x03, 500},        /* 500 XP gained */
        {1004, "Skill Master", 0x04, 10},      /* 10 skills used */
        {1005, "Efficiency Expert", 0x05, 80}, /* 80% efficiency score */
    };
    
    for (size_t i = 0; i < sizeof(achievement_definitions) / sizeof(achievement_definitions[0]) && 
         bridge->achievement_count < 64; i++) {
        
        bool should_trigger = false;
        uint32_t progress_value = 0;
        
        /* Check trigger conditions based on event type */
        switch (achievement_definitions[i].trigger_type) {
            case 0x01: /* Enemy defeated */
                should_trigger = (combat_event_type == 0x01);
                progress_value = event_data;
                break;
            case 0x02: /* Damage dealt threshold */
                should_trigger = (combat_event_type == 0x02);
                progress_value = bridge->total_xp_awarded_session;
                break;
            case 0x03: /* XP gained threshold */
                should_trigger = (bridge->total_xp_awarded_session >= achievement_definitions[i].required_value);
                progress_value = bridge->total_xp_awarded_session;
                break;
            case 0x04: /* Skill usage threshold */
                should_trigger = (bridge->total_skill_activations >= achievement_definitions[i].required_value);
                progress_value = bridge->total_skill_activations;
                break;
            case 0x05: /* Efficiency threshold */
                should_trigger = (bridge->efficiency_metrics.overall_efficiency_score >= 
                                  achievement_definitions[i].required_value);
                progress_value = (uint32_t)bridge->efficiency_metrics.overall_efficiency_score;
                break;
        }
        
        if (should_trigger && progress_value >= achievement_definitions[i].required_value) {
            /* Check if achievement already exists */
            bool already_unlocked = false;
            for (uint8_t j = 0; j < bridge->achievement_count; j++) {
                if (bridge->achievements[j].achievement_id == achievement_definitions[i].id) {
                    already_unlocked = true;
                    break;
                }
            }
            
            if (!already_unlocked) {
                /* Unlock new achievement */
                RogueCombatAchievement* ach = &bridge->achievements[bridge->achievement_count];
                ach->achievement_id = achievement_definitions[i].id;
                strncpy_s(ach->achievement_name, sizeof(ach->achievement_name), 
                          achievement_definitions[i].name, _TRUNCATE);
                ach->trigger_condition = achievement_definitions[i].trigger_type;
                ach->progress_current = progress_value;
                ach->progress_required = achievement_definitions[i].required_value;
                ach->just_unlocked = true;
                ach->reward_xp = achievement_definitions[i].required_value / 10; /* 10% bonus XP */
                
                bridge->achievement_count++;
                bridge->achievements_unlocked_session++;
                achievements_triggered++;
                
                /* Publish achievement event */
                RogueEventPayload payload = {0};
                memcpy(payload.raw_data, &ach->achievement_id, sizeof(uint32_t));
                rogue_event_publish(ROGUE_COMBAT_PROG_EVENT_ACHIEVEMENT_UNLOCKED,
                                    &payload, ROGUE_EVENT_PRIORITY_HIGH,
                                    0, "CombatProgressionBridge");
                
                BRIDGE_LOG(bridge, "INFO", "🏆 Achievement unlocked: %s (ID: %u, Reward: %u XP)",
                           ach->achievement_name, ach->achievement_id, ach->reward_xp);
            }
        }
    }
    
    bridge->metrics.achievements_triggered += achievements_triggered;
    
    return (int)achievements_triggered;
}

int rogue_combat_progression_bridge_get_recent_achievements(RogueCombatProgressionBridge* bridge,
                                                             RogueCombatAchievement* out_achievements,
                                                             uint8_t max_count) {
    if (!bridge || !bridge->initialized || !out_achievements || max_count == 0) {
        return 0;
    }
    
    uint8_t recent_count = 0;
    
    /* Find achievements that were just unlocked */
    for (uint8_t i = 0; i < bridge->achievement_count && recent_count < max_count; i++) {
        if (bridge->achievements[i].just_unlocked) {
            out_achievements[recent_count] = bridge->achievements[i];
            bridge->achievements[i].just_unlocked = false; /* Clear the flag */
            recent_count++;
        }
    }
    
    return recent_count;
}

/* Phase 3.3.5: Playstyle Analysis Implementation */
int rogue_combat_progression_bridge_analyze_playstyle(RogueCombatProgressionBridge* bridge,
                                                       uint32_t damage_type_preference,
                                                       uint32_t risk_behavior,
                                                       uint32_t tactical_complexity) {
    if (!bridge || !bridge->initialized) {
        return 0;
    }
    
    /* Update playstyle data points */
    bridge->playstyle_data_points[0] += damage_type_preference; /* Physical vs magical */
    bridge->playstyle_data_points[1] += risk_behavior;          /* Risk tolerance */
    bridge->playstyle_data_points[2] += tactical_complexity;    /* Tactical usage */
    bridge->playstyle_data_points[3]++; /* Combat sessions count */
    
    /* Analyze playstyle every 10 combat sessions */
    if (bridge->playstyle_data_points[3] % 10 == 0) {
        float avg_damage_pref = (float)bridge->playstyle_data_points[0] / bridge->playstyle_data_points[3];
        float avg_risk = (float)bridge->playstyle_data_points[1] / bridge->playstyle_data_points[3];
        float avg_tactical = (float)bridge->playstyle_data_points[2] / bridge->playstyle_data_points[3];
        
        /* Determine playstyle based on averages */
        RogueCombatPlaystyle detected_style = ROGUE_PLAYSTYLE_BALANCED;
        float confidence = 0.0f;
        
        if (avg_risk > 70.0f && avg_damage_pref > 60.0f) {
            detected_style = ROGUE_PLAYSTYLE_AGGRESSIVE;
            confidence = 0.8f;
        } else if (avg_risk < 30.0f && avg_tactical > 50.0f) {
            detected_style = ROGUE_PLAYSTYLE_DEFENSIVE;
            confidence = 0.75f;
        } else if (avg_tactical > 80.0f) {
            detected_style = ROGUE_PLAYSTYLE_TACTICAL;
            confidence = 0.85f;
        } else if (damage_type_preference > 80.0f) { /* High magical preference */
            detected_style = ROGUE_PLAYSTYLE_MAGICAL;
            confidence = 0.7f;
        } else {
            detected_style = ROGUE_PLAYSTYLE_BALANCED;
            confidence = 0.6f;
        }
        
        /* Update playstyle profile */
        bridge->playstyle_profile.detected_style = detected_style;
        bridge->playstyle_profile.confidence_score = confidence;
        bridge->playstyle_profile.combat_sessions_analyzed = bridge->playstyle_data_points[3];
        bridge->playstyle_profile.damage_preference = (uint32_t)avg_damage_pref;
        bridge->playstyle_profile.risk_tolerance = (uint32_t)avg_risk;
        bridge->playstyle_profile.tactical_usage = (uint32_t)avg_tactical;
        
        bridge->metrics.playstyle_analyses_performed++;
        
        /* Publish playstyle detection event */
        RogueEventPayload payload = {0};
        uint32_t playstyle_data = ((uint32_t)detected_style << 16) | (uint32_t)(confidence * 100);
        memcpy(payload.raw_data, &playstyle_data, sizeof(uint32_t));
        rogue_event_publish(ROGUE_COMBAT_PROG_EVENT_PLAYSTYLE_DETECTED,
                            &payload, ROGUE_EVENT_PRIORITY_LOW,
                            0, "CombatProgressionBridge");
        
        const char* style_names[] = {"Aggressive", "Defensive", "Balanced", "Tactical", "Magical"};
        BRIDGE_LOG(bridge, "INFO", "🎯 Playstyle detected: %s (%.1f%% confidence) after %u sessions",
                   style_names[detected_style], confidence * 100.0f, 
                   bridge->playstyle_data_points[3]);
    }
    
    return 1;
}

RogueCombatPlaystyle rogue_combat_progression_bridge_get_detected_playstyle(RogueCombatProgressionBridge* bridge,
                                                                             float* out_confidence) {
    if (!bridge || !bridge->initialized) {
        if (out_confidence) *out_confidence = 0.0f;
        return ROGUE_PLAYSTYLE_BALANCED;
    }
    
    if (out_confidence) {
        *out_confidence = bridge->playstyle_profile.confidence_score;
    }
    
    return bridge->playstyle_profile.detected_style;
}

/* Phase 3.3.6: Efficiency Metrics Implementation */
int rogue_combat_progression_bridge_update_efficiency_metrics(RogueCombatProgressionBridge* bridge,
                                                               float combat_duration_ms,
                                                               uint32_t damage_dealt,
                                                               uint32_t mana_used,
                                                               uint32_t damage_taken) {
    if (!bridge || !bridge->initialized || combat_duration_ms <= 0) {
        return 0;
    }
    
    RogueCombatEfficiencyMetrics* metrics = &bridge->efficiency_metrics;
    
    /* Calculate current combat metrics */
    float current_dps = damage_dealt / (combat_duration_ms / 1000.0f);
    float current_dpm = (mana_used > 0) ? (damage_dealt / (float)mana_used) : damage_dealt;
    float current_resource_eff = (damage_taken == 0) ? 100.0f : 
                                 ((float)damage_dealt / (damage_dealt + damage_taken)) * 100.0f;
    
    /* Update running averages */
    metrics->damage_per_second_avg = (metrics->damage_per_second_avg + current_dps) / 2.0f;
    metrics->damage_per_mana_efficiency = (metrics->damage_per_mana_efficiency + current_dpm) / 2.0f;
    metrics->time_to_kill_avg_ms = (metrics->time_to_kill_avg_ms + combat_duration_ms) / 2.0f;
    metrics->resource_usage_efficiency = (metrics->resource_usage_efficiency + current_resource_eff) / 2.0f;
    
    /* Update counters */
    if (damage_taken == 0) {
        metrics->perfect_combat_count++;
    }
    
    /* Calculate overall efficiency score (weighted average) */
    float dps_score = fminf(metrics->damage_per_second_avg / 10.0f, 100.0f); /* Cap at 100 */
    float resource_score = metrics->resource_usage_efficiency;
    float time_score = fmaxf(0.0f, 100.0f - (metrics->time_to_kill_avg_ms / 1000.0f)); /* Faster = better */
    
    metrics->overall_efficiency_score = (dps_score * 0.4f + resource_score * 0.4f + time_score * 0.2f);
    
    bridge->metrics.efficiency_calculations_performed++;
    
    /* Check for efficiency milestones */
    uint32_t milestone_thresholds[] = {60, 70, 80, 90, 95};
    for (size_t i = 0; i < sizeof(milestone_thresholds) / sizeof(milestone_thresholds[0]); i++) {
        if (metrics->overall_efficiency_score >= milestone_thresholds[i] && 
            bridge->efficiency_milestone_count <= i) {
            
            bridge->efficiency_milestone_count = (uint32_t)(i + 1);
            
            /* Publish efficiency milestone event */
            RogueEventPayload payload = {0};
            uint32_t milestone_data = milestone_thresholds[i];
            memcpy(payload.raw_data, &milestone_data, sizeof(uint32_t));
            rogue_event_publish(ROGUE_COMBAT_PROG_EVENT_EFFICIENCY_MILESTONE,
                                &payload, ROGUE_EVENT_PRIORITY_NORMAL,
                                0, "CombatProgressionBridge");
            
            BRIDGE_LOG(bridge, "INFO", "⚡ Efficiency milestone reached: %u%% overall efficiency!",
                       milestone_thresholds[i]);
        }
    }
    
    BRIDGE_LOG(bridge, "DEBUG", "Efficiency updated: DPS=%.1f, Resource=%.1f%%, Overall=%.1f%%",
               current_dps, current_resource_eff, metrics->overall_efficiency_score);
    
    return 1;
}

int rogue_combat_progression_bridge_get_efficiency_score(RogueCombatProgressionBridge* bridge,
                                                          float* out_overall_score,
                                                          RogueCombatEfficiencyMetrics* out_detailed) {
    if (!bridge || !bridge->initialized) {
        return 0;
    }
    
    if (out_overall_score) {
        *out_overall_score = bridge->efficiency_metrics.overall_efficiency_score;
    }
    
    if (out_detailed) {
        *out_detailed = bridge->efficiency_metrics;
    }
    
    return 1;
}

/* Phase 3.3.7: Death Penalty Implementation */
int rogue_combat_progression_bridge_apply_death_penalty(RogueCombatProgressionBridge* bridge,
                                                         uint32_t player_level,
                                                         uint32_t current_xp) {
    if (!bridge || !bridge->initialized) {
        return 0;
    }
    
    RogueCombatDeathPenalty* penalty = &bridge->death_penalty_state;
    
    penalty->death_count++;
    
    /* Calculate XP penalty (5-10% based on level, minimum 100 XP) */
    penalty->xp_penalty_percentage = 5.0f + (player_level / 10.0f);
    penalty->xp_penalty_percentage = fminf(penalty->xp_penalty_percentage, 10.0f);
    penalty->xp_penalty_amount = (uint32_t)fmaxf(current_xp * (penalty->xp_penalty_percentage / 100.0f), 100.0f);
    
    /* Calculate equipment durability loss (10-20% based on death count) */
    penalty->equipment_durability_loss = 10 + (penalty->death_count % 10);
    
    /* Calculate currency penalty (1% of current currency, estimated) */
    penalty->currency_penalty = player_level * 100; /* Rough estimate */
    
    /* Apply temporary skill penalty (reduces effectiveness for 5 minutes) */
    penalty->skill_penalty_duration_ms = 300000; /* 5 minutes */
    
    /* Check for resurrection items (placeholder) */
    penalty->resurrection_items_consumed = false; /* Would check inventory */
    
    bridge->metrics.death_penalties_applied++;
    
    /* Publish death penalty event */
    RogueEventPayload payload = {0};
    memcpy(payload.raw_data, &penalty->xp_penalty_amount, sizeof(uint32_t));
    rogue_event_publish(ROGUE_COMBAT_PROG_EVENT_DEATH_PENALTY_APPLIED,
                        &payload, ROGUE_EVENT_PRIORITY_HIGH,
                        0, "CombatProgressionBridge");
    
    BRIDGE_LOG(bridge, "WARN", "💀 Death penalty applied: -%u XP (%.1f%%), -%u%% durability, -%u currency",
               penalty->xp_penalty_amount, penalty->xp_penalty_percentage,
               penalty->equipment_durability_loss, penalty->currency_penalty);
    
    return 1;
}

int rogue_combat_progression_bridge_get_death_penalty_info(RogueCombatProgressionBridge* bridge,
                                                            RogueCombatDeathPenalty* out_penalty_info) {
    if (!bridge || !bridge->initialized || !out_penalty_info) {
        return 0;
    }
    
    *out_penalty_info = bridge->death_penalty_state;
    
    return 1;
}

/* Debug & Utility Functions */
void rogue_combat_progression_bridge_set_debug_mode(RogueCombatProgressionBridge* bridge, bool enabled) {
    if (bridge) {
        bridge->debug_mode = enabled;
        BRIDGE_LOG(bridge, "INFO", "Debug mode %s", enabled ? "enabled" : "disabled");
    }
}

void rogue_combat_progression_bridge_get_metrics(RogueCombatProgressionBridge* bridge, char* out_buffer, size_t buffer_size) {
    if (!bridge || !out_buffer || buffer_size == 0) {
        return;
    }
    
    snprintf(out_buffer, buffer_size,
             "Combat-Progression Bridge Metrics:\n"
             "  XP Distributions: %u (Total: %u XP)\n"
             "  Skill Usage Events: %u (Total Activations: %u)\n"
             "  Passive Effects Applied: %u (Active: %u)\n"
             "  Achievements Triggered: %u (Session: %u)\n"
             "  Playstyle Analyses: %u (Current: %s)\n"
             "  Efficiency Calculations: %u (Score: %.1f%%)\n"
             "  Death Penalties Applied: %u\n"
             "  Avg Processing Time: %.3f ms\n"
             "  Errors: %u\n",
             bridge->metrics.xp_distributions_processed,
             bridge->total_xp_awarded_session,
             bridge->metrics.skill_usage_events_processed,
             bridge->total_skill_activations,
             bridge->metrics.passive_effects_applied,
             bridge->active_passives_count,
             bridge->metrics.achievements_triggered,
             bridge->achievements_unlocked_session,
             bridge->metrics.playstyle_analyses_performed,
             (bridge->playstyle_profile.detected_style < ROGUE_PLAYSTYLE_COUNT) ? 
             (const char*[]){"Aggressive", "Defensive", "Balanced", "Tactical", "Magical"}[bridge->playstyle_profile.detected_style] : "Unknown",
             bridge->metrics.efficiency_calculations_performed,
             bridge->efficiency_metrics.overall_efficiency_score,
             bridge->metrics.death_penalties_applied,
             bridge->metrics.avg_processing_time_ms,
             bridge->error_count);
}

void rogue_combat_progression_bridge_reset_metrics(RogueCombatProgressionBridge* bridge) {
    if (!bridge || !bridge->initialized) {
        return;
    }
    
    memset(&bridge->metrics, 0, sizeof(bridge->metrics));
    bridge->xp_distribution_count = 0;
    bridge->total_xp_awarded_session = 0;
    bridge->achievements_unlocked_session = 0;
    bridge->error_count = 0;
    memset(bridge->last_error, 0, sizeof(bridge->last_error));
    
    BRIDGE_LOG(bridge, "INFO", "Combat-Progression Bridge metrics reset");
}
