/* AI Debug & Tooling (Phase 10.1 - 10.5)
   Provides:
   - Behavior tree ASCII visualizer
   - Blackboard dump helper
   - Perception overlay primitive collection
   - Trace buffer JSON export
   - Determinism verifier (dual tree factory run hash compare)
*/
#ifndef ROGUE_AI_DEBUG_H
#define ROGUE_AI_DEBUG_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" { 
#endif

struct RogueBehaviorTree; struct RogueBlackboard; struct RoguePerceptionAgent; struct RogueAITraceBuffer;

/* 10.1 Behavior tree visualization: writes ASCII tree lines into out (null terminated). Returns bytes written (excl null) or -1. */
int rogue_ai_bt_visualize(struct RogueBehaviorTree* tree, char* out, int cap);

/* 10.3 Blackboard inspector: dumps key=value entries line separated. */
int rogue_ai_blackboard_dump(struct RogueBlackboard* bb, char* out, int cap);

/* 10.2 Perception overlay primitives */
typedef enum RogueAIDebugPrimKind { ROGUE_AI_DEBUG_PRIM_FOV_CONE = 1, ROGUE_AI_DEBUG_PRIM_LOS_RAY = 2 } RogueAIDebugPrimKind;
typedef struct RogueAIDebugPrimitive { float ax, ay, bx, by; uint8_t kind; } RogueAIDebugPrimitive;
/* Collect debug primitives for a single agent relative to player (player_x,y). Returns count written. */
int rogue_ai_perception_collect_debug(const struct RoguePerceptionAgent* agent, float player_x, float player_y,
                                      RogueAIDebugPrimitive* out, int max, float fov_deg, float vision_dist);

/* 10.4 Trace export to JSON array string: [{"tick":N,"hash":H},...] */
int rogue_ai_trace_export_json(const struct RogueAITraceBuffer* tb, char* out, int cap);

/* 10.5 Determinism verifier: builds two trees via factory, ticks each 'ticks' times and hashes per-tick serialized path.
   Returns 1 if hashes match (deterministic), 0 otherwise. out_hash optional aggregate hash. */
typedef struct RogueBehaviorTree* (*RogueAIBTFactory)(void);
int rogue_ai_determinism_verify(RogueAIBTFactory factory, int ticks, uint64_t* out_hash);

#ifdef __cplusplus
}
#endif
#endif
