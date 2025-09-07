#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_dungeon_objectives.h"
#include <stdio.h>

/* Validate dynamic substitution when no puzzle-tagged rooms exist: PUZZLE becomes CLEAR. */
int main(void)
{
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 9999u;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);

    RogueDungeonGenParams p = {0};
    p.target_rooms = 12;
    p.loop_percent = 10;
    p.arch = ROGUE_DUNGEON_ARCH_BRANCHING;

    RogueDungeonGraph g = {0};
    if (!rogue_dungeon_generate_graph_ex(&ctx, &p, &g))
    {
        fprintf(stderr, "graph gen failed\n");
        return 1;
    }

    /* Clear puzzle tags so that optional/required puzzle will trigger substitution/omission */
    for (int i = 0; i < g.room_count; ++i)
        g.rooms[i].tag &= ~ROGUE_DUNGEON_ROOM_PUZZLE;

    rogue_dungeon_set_objective_script("ACTIVATE,PUZZLE,CLEAR,BOSS");
    RogueDungeonObjectiveStep steps[8];
    int k = rogue_dungeon_build_objectives(&ctx, &g, 4, steps, 8);
    if (k < 3)
    {
        fprintf(stderr, "expected >=3 steps, got %d\n", k);
        rogue_dungeon_free_graph(&g);
        return 2;
    }

    int has_clear = 0, has_puzzle = 0;
    for (int i = 0; i < k; ++i)
    {
        if (steps[i].type == ROGUE_OBJ_CLEAR)
            has_clear = 1;
        if (steps[i].type == ROGUE_OBJ_PUZZLE_COMPLETE)
            has_puzzle = 1;
    }
    if (!has_clear)
    {
        fprintf(stderr, "expected CLEAR substitution when no puzzle exists\n");
        rogue_dungeon_free_graph(&g);
        return 3;
    }
    if (has_puzzle)
    {
        fprintf(stderr, "unexpected PUZZLE when none exists\n");
        rogue_dungeon_free_graph(&g);
        return 4;
    }

    rogue_dungeon_free_graph(&g);
    return 0;
}
