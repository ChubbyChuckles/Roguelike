#ifndef ROGUE_WORLD_GEN_DUNGEON_ENCOUNTERS_H
#define ROGUE_WORLD_GEN_DUNGEON_ENCOUNTERS_H

#include "world_gen.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RogueDungeonEncounterType
    {
        ROGUE_ENC_COMBAT = 0,
        ROGUE_ENC_ELITE_PACK = 1,
        ROGUE_ENC_MINI_BOSS = 2,
        ROGUE_ENC_PUZZLE_GUARD = 3
    } RogueDungeonEncounterType;

    typedef struct RogueDungeonEncounterPlanEntry
    {
        int room_id;                    /* target room id */
        RogueDungeonEncounterType type; /* encounter type */
        int budget;                     /* abstract difficulty budget units */
        int modifiers_mask;             /* reserved for future affixes/modifiers */
        int nemesis;                    /* 1 if nemesis injected for the room */
    } RogueDungeonEncounterPlanEntry;

    /* Plans one encounter per room. Returns number of entries written (<=max_entries). */
    int rogue_dungeon_plan_encounters(RogueWorldGenContext* ctx, const RogueDungeonGraph* graph,
                                      int depth, RogueDungeonEncounterPlanEntry* out,
                                      int max_entries);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WORLD_GEN_DUNGEON_ENCOUNTERS_H */
