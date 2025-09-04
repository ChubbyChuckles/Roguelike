#include "world/world_gen.h"
#include "world/world_gen_dungeon_objectives.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 777u;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);

    RogueDungeonGenParams p = {0};
    p.target_rooms = 18;
    p.loop_percent = 20;
    p.arch = ROGUE_DUNGEON_ARCH_BRANCHING;

    RogueDungeonGraph g = {0};
    if (!rogue_dungeon_generate_graph_ex(&ctx, &p, &g))
    {
        fprintf(stderr, "graph gen failed\n");
        return 1;
    }

    /* Script: include optional puzzle, add gate/keystone pair before clear, then boss */
    rogue_dungeon_set_objective_script("ACTIVATE,?PUZZLE,GATE(KEYSTONE),CLEAR,BOSS");

    RogueDungeonObjectiveStep steps[10];
    int k = rogue_dungeon_build_objectives(&ctx, &g, 5, steps, 10);
    if (k < 4)
    {
        fprintf(stderr, "expected >=4 steps, got %d\n", k);
        rogue_dungeon_free_graph(&g);
        return 2;
    }

    /* Gate expansion should yield a COLLECT(KEYSTONE) followed by ACTIVATE(GATE) in order */
    int found_collect = 0, found_gate = 0;
    for (int i = 0; i < k; ++i)
    {
        if (steps[i].type == ROGUE_OBJ_COLLECT && (steps[i].param0 & ROGUE_OBJ_PARAM_KEYSTONE))
            found_collect = 1;
        if (steps[i].type == ROGUE_OBJ_ACTIVATE && (steps[i].param0 & ROGUE_OBJ_PARAM_GATE))
            found_gate = 1;
    }
    if (!found_collect || !found_gate)
    {
        fprintf(stderr, "gate/keystone pair not found in sequence\n");
        rogue_dungeon_free_graph(&g);
        return 3;
    }

    /* Boss must be terminal */
    if (steps[k - 1].type != ROGUE_OBJ_BOSS)
    {
        fprintf(stderr, "last step not BOSS\n");
        rogue_dungeon_free_graph(&g);
        return 4;
    }

    /* Determinism: re-run and compare exact sequence (types + param0 flags) */
    RogueDungeonObjectiveStep steps2[10];
    rogue_worldgen_context_init(&ctx, &cfg);
    RogueDungeonGraph g2 = {0};
    if (!rogue_dungeon_generate_graph_ex(&ctx, &p, &g2))
    {
        fprintf(stderr, "graph2 gen failed\n");
        rogue_dungeon_free_graph(&g);
        return 5;
    }
    rogue_dungeon_set_objective_script("ACTIVATE,?PUZZLE,GATE(KEYSTONE),CLEAR,BOSS");
    int k2 = rogue_dungeon_build_objectives(&ctx, &g2, 5, steps2, 10);
    if (k != k2)
    {
        fprintf(stderr, "step count mismatch %d vs %d\n", k, k2);
        rogue_dungeon_free_graph(&g);
        rogue_dungeon_free_graph(&g2);
        return 6;
    }
    for (int i = 0; i < k; ++i)
    {
        if (steps[i].type != steps2[i].type || steps[i].room_id != steps2[i].room_id ||
            steps[i].param0 != steps2[i].param0)
        {
            fprintf(stderr, "determinism mismatch at step %d\n", i);
            rogue_dungeon_free_graph(&g);
            rogue_dungeon_free_graph(&g2);
            return 7;
        }
    }

    rogue_dungeon_free_graph(&g);
    rogue_dungeon_free_graph(&g2);
    return 0;
}
