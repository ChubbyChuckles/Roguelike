#ifndef ROGUE_AI_ADVANCED_NODES_H
#define ROGUE_AI_ADVANCED_NODES_H

#include "../core/behavior_tree.h"
#include "../core/blackboard.h"
#include "../perception/perception.h"
#include "../util/utility_scorer.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Phase 4 Node Library (subset 4.1,4.2,4.3,4.5,4.6) */

    /* Parallel node: succeeds when all children succeed, fails if any fails; running if any running
     */
    RogueBTNode* rogue_bt_parallel(const char* name);

    /* Utility selector: evaluates child scorers, ticks highest scoring child (ties choose first).
     */
    RogueBTNode* rogue_bt_utility_selector(const char* name);
    /* Attach scorer to child node (must be child of utility selector). Returns 1 on success. */
    int rogue_bt_utility_set_child_scorer(RogueBTNode* utility_node, RogueBTNode* child,
                                          RogueUtilityScorer scorer);

    /* Condition Nodes */
    RogueBTNode* rogue_bt_condition_player_visible(const char* name, const char* bb_player_pos_key,
                                                   const char* bb_agent_pos_key,
                                                   const char* bb_agent_facing_key, float fov_deg,
                                                   float max_dist);
    RogueBTNode* rogue_bt_condition_timer_elapsed(const char* name, const char* bb_timer_key,
                                                  float min_value);
    RogueBTNode* rogue_bt_condition_health_below(const char* name, const char* bb_health_key,
                                                 float threshold);

    /* Action Nodes (stubs update blackboard markers) */
    RogueBTNode* rogue_bt_action_move_to(const char* name, const char* bb_target_pos_key,
                                         const char* bb_agent_pos_key, float speed,
                                         const char* bb_out_reached_flag);
    RogueBTNode* rogue_bt_action_flee_from(const char* name, const char* bb_threat_pos_key,
                                           const char* bb_agent_pos_key, float speed);
    RogueBTNode* rogue_bt_action_attack_melee(const char* name, const char* bb_in_range_flag_key,
                                              const char* bb_cooldown_timer_key,
                                              float cooldown_seconds);
    RogueBTNode* rogue_bt_action_attack_ranged(const char* name, const char* bb_line_clear_flag_key,
                                               const char* bb_cooldown_timer_key,
                                               float cooldown_seconds);
    /* Phase 6.1: Fire a projectile from agent toward target if optional line-clear flag is true.
        Resets optional cooldown timer to 0 on fire. Speed/life/damage are constants for now. */
    RogueBTNode* rogue_bt_action_ranged_fire_projectile(
        const char* name, const char* bb_agent_pos_key, const char* bb_target_pos_key,
        const char* bb_optional_line_clear_flag_key, const char* bb_optional_cooldown_timer_key,
        float speed_tiles_per_sec, float life_ms, int damage);
    RogueBTNode* rogue_bt_action_strafe(const char* name, const char* bb_target_pos_key,
                                        const char* bb_agent_pos_key,
                                        const char* bb_strafe_left_flag_key, float speed,
                                        float duration_seconds);

    /* Phase 6.2: Reaction windows */
    /* Parry reaction: when incoming_threat_flag is true, activates parry by setting
       out_parry_active to true and starting/resetting a timer. While the timer is below
       window_seconds, returns SUCCESS and keeps parry active; after the window elapses, deactivates
       parry and returns FAILURE. */
    RogueBTNode* rogue_bt_action_react_parry(const char* name,
                                             const char* bb_incoming_threat_flag_key,
                                             const char* bb_out_parry_active_key,
                                             const char* bb_parry_timer_key, float window_seconds);

    /* Dodge reaction: when incoming_threat_flag is true, activates dodge for duration_seconds and
        writes a normalized dodge vector away from threat (computed from agent_pos and threat_pos)
        to bb_out_dodge_vec_key. While active returns SUCCESS; after elapsing returns FAILURE. */
    RogueBTNode*
    rogue_bt_action_react_dodge(const char* name, const char* bb_incoming_threat_flag_key,
                                const char* bb_agent_pos_key, const char* bb_threat_pos_key,
                                const char* bb_out_dodge_vec_key, const char* bb_dodge_timer_key,
                                float duration_seconds);

    /* Phase 6.3: Opportunistic attack when target is in recovery window
        Succeeds if a boolean recovery flag is true and optional distance constraint is satisfied.
        Resets an optional cooldown timer on success. */
    RogueBTNode* rogue_bt_action_opportunistic_attack(
        const char* name, const char* bb_target_in_recovery_flag_key, const char* bb_agent_pos_key,
        const char* bb_target_pos_key, float max_distance_allowed, /* <=0 to ignore */
        const char* bb_optional_cooldown_timer_key);

    /* Phase 6.4: Kiting logic — maintain a preferred distance band from target.
        Moves away if closer than min, toward if farther than max, SUCCESS when within [min,max]. */
    RogueBTNode* rogue_bt_action_kite_band(const char* name, const char* bb_agent_pos_key,
                                           const char* bb_target_pos_key,
                                           float preferred_min_distance,
                                           float preferred_max_distance, float move_speed);

    /* Tactical Nodes */
    RogueBTNode* rogue_bt_tactical_flank_attempt(const char* name, const char* bb_player_pos_key,
                                                 const char* bb_agent_pos_key,
                                                 const char* bb_out_flank_target_key, float offset);
    RogueBTNode* rogue_bt_tactical_regroup(const char* name, const char* bb_regroup_point_key,
                                           const char* bb_agent_pos_key, float speed);
    RogueBTNode* rogue_bt_tactical_cover_seek(const char* name, const char* bb_player_pos_key,
                                              const char* bb_agent_pos_key,
                                              const char* bb_obstacle_pos_key,
                                              const char* bb_out_cover_point_key,
                                              const char* bb_out_in_cover_flag_key,
                                              float obstacle_radius, float move_speed);

    /* Decorators */
    RogueBTNode* rogue_bt_decorator_cooldown(const char* name, RogueBTNode* child,
                                             const char* bb_timer_key, float cooldown_seconds);
    RogueBTNode* rogue_bt_decorator_retry(const char* name, RogueBTNode* child, int max_attempts);

    /* Decorator: Stuck detection. Monitors an agent vec2 position; if position hasn't advanced
        beyond threshold for window_seconds, returns FAILURE and resets its window timer. Otherwise
        ticks child and passes through child's status. */
    RogueBTNode* rogue_bt_decorator_stuck_detect(const char* name, RogueBTNode* child,
                                                 const char* bb_agent_pos_key,
                                                 const char* bb_window_timer_key,
                                                 float window_seconds, float min_move_threshold);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_AI_ADVANCED_NODES_H */
