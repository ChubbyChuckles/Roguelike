#ifndef ROGUE_WORLD_GEN_DUNGEON_OBJECTIVES_H
#define ROGUE_WORLD_GEN_DUNGEON_OBJECTIVES_H

#include "world_gen.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Phase 5: Objectives & Progression Gates (minimal, deterministic slice)
     * Contract
     * inputs: ctx (seeded), graph (rooms, edges), depth
     * outputs: out[K<=max_steps] ordered objective steps culminating in BOSS
     * success: returns count >= 3
     * error modes: invalid args -> 0
     */

    typedef enum RogueDungeonObjectiveType
    {
        ROGUE_OBJ_CLEAR = 0,
        ROGUE_OBJ_COLLECT = 1,
        ROGUE_OBJ_ACTIVATE = 2,
        ROGUE_OBJ_ESCORT = 3,
        ROGUE_OBJ_SURVIVE = 4,
        ROGUE_OBJ_PUZZLE_COMPLETE = 5,
        ROGUE_OBJ_BOSS = 6
    } RogueDungeonObjectiveType;

    typedef struct RogueDungeonObjectiveStep
    {
        int step_index;                 /* 0..K-1 in sequence */
        RogueDungeonObjectiveType type; /* objective kind */
        int room_id;                    /* target room id for the step */
        int param0;                     /* reserved for future data (e.g., count) */
    } RogueDungeonObjectiveStep;

    /* Build a simple objective chain based on room depths and tags.
     * Strategy: shallow ACTIVATE (or CLEAR), optional PUZZLE_COMPLETE if a puzzle room exists,
     * mid-depth CLEAR, and terminal BOSS in the farthest room by BFS depth.
     */
    int rogue_dungeon_build_objectives(RogueWorldGenContext* ctx, const RogueDungeonGraph* graph,
                                       int depth, RogueDungeonObjectiveStep* out, int max_steps);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WORLD_GEN_DUNGEON_OBJECTIVES_H */
