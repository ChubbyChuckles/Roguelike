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
    RogueBTNode* rogue_bt_action_strafe(const char* name, const char* bb_target_pos_key,
                                        const char* bb_agent_pos_key,
                                        const char* bb_strafe_left_flag_key, float speed,
                                        float duration_seconds);

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
