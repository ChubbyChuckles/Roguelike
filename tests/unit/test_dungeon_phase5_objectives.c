#include "world/world_gen.h"
#include "world/world_gen_dungeon_kernel.h"
#include "world/world_gen_dungeon_objectives.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 424242u;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);

    RogueDungeonGenParams p = {0};
    p.target_rooms = 20;
    p.loop_percent = 15;
    p.arch = ROGUE_DUNGEON_ARCH_BRANCHING;

    RogueDungeonGraph g = {0};
    if (!rogue_dungeon_generate_graph_ex(&ctx, &p, &g))
    {
        fprintf(stderr, "graph gen failed\n");
        return 1;
    }

    RogueDungeonObjectiveStep steps[8];
    int k = rogue_dungeon_build_objectives(&ctx, &g, /*depth*/ 6, steps, 8);
    if (k < 3)
    {
        fprintf(stderr, "expected >=3 steps, got %d\n", k);
        rogue_dungeon_free_graph(&g);
        return 2;
    }
    if (steps[k - 1].type != ROGUE_OBJ_BOSS)
    {
        fprintf(stderr, "last step not BOSS\n");
        rogue_dungeon_free_graph(&g);
        return 3;
    }
    /* Determinism: regenerate and compare */
    RogueDungeonGraph g2 = {0};
    rogue_worldgen_context_init(&ctx, &cfg);
    if (!rogue_dungeon_generate_graph_ex(&ctx, &p, &g2))
    {
        fprintf(stderr, "graph2 gen failed\n");
        rogue_dungeon_free_graph(&g);
        return 4;
    }
    RogueDungeonObjectiveStep steps2[8];
    int k2 = rogue_dungeon_build_objectives(&ctx, &g2, 6, steps2, 8);
    if (k != k2)
    {
        fprintf(stderr, "step count mismatch %d vs %d\n", k, k2);
        rogue_dungeon_free_graph(&g);
        rogue_dungeon_free_graph(&g2);
        return 5;
    }
    for (int i = 0; i < k; ++i)
    {
        if (steps[i].type != steps2[i].type || steps[i].room_id != steps2[i].room_id)
        {
            fprintf(stderr, "determinism mismatch at step %d\n", i);
            rogue_dungeon_free_graph(&g);
            rogue_dungeon_free_graph(&g2);
            return 6;
        }
    }

    /* Basic gate integrity: all referenced room_ids should exist in the graph */
    for (int i = 0; i < k; ++i)
    {
        int found = 0;
        for (int r = 0; r < g.room_count; ++r)
        {
            if (g.rooms[r].id == steps[i].room_id)
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            fprintf(stderr, "step references unknown room id %d\n", steps[i].room_id);
            rogue_dungeon_free_graph(&g);
            rogue_dungeon_free_graph(&g2);
            return 7;
        }
    }

    rogue_dungeon_free_graph(&g);
    rogue_dungeon_free_graph(&g2);
    return 0;
}
