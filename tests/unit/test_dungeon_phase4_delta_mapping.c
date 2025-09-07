#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_dungeon_encounters.h"
#include "../../src/world/world_gen_dungeon_taxonomy.h"
#include <stdio.h>

/* ΔL precision tests: ensure mapping steps every 3 depths up to clamp and budgets reflect ΔL trend.
 */
int main(void)
{
    /* Verify mapping: depth->ΔL increments each 3 levels and saturates at <=8. */
    int prev = -1;
    for (int d = 0; d <= 30; ++d)
    {
        int dl = rogue_dungeon_target_level_delta(d);
        if (dl < 0 || dl > 8)
        {
            fprintf(stderr, "ΔL out of bounds at depth %d => %d\n", d, dl);
            return 1;
        }
        if (d > 0 && (d % 3) == 0)
        {
            if (dl < prev)
            {
                fprintf(stderr, "ΔL decreased at depth %d: %d -> %d\n", d, prev, dl);
                return 2;
            }
        }
        prev = dl;
    }

    /* Budget trend sanity: deeper graphs with higher ΔL should not yield systematically lower base
     * budgets. */
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 9001u;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);

    RogueDungeonGenParams p = {0};
    p.target_rooms = 18;
    p.loop_percent = 10;
    p.arch = ROGUE_DUNGEON_ARCH_LINEAR;

    RogueDungeonGraph g = {0};
    if (!rogue_dungeon_generate_graph_ex(&ctx, &p, &g))
    {
        fprintf(stderr, "graph gen failed\n");
        return 3;
    }

    RogueDungeonEncounterPlanEntry e1[256];
    RogueDungeonEncounterPlanEntry e2[256];
    int n1 = rogue_dungeon_plan_encounters(&ctx, &g, 3, e1, 256);
    int n2 = rogue_dungeon_plan_encounters(&ctx, &g, 9, e2, 256);
    if (n1 != g.room_count || n2 != g.room_count)
    {
        fprintf(stderr, "plan count mismatch\n");
        rogue_dungeon_free_graph(&g);
        return 4;
    }
    long sum1 = 0, sum2 = 0;
    for (int i = 0; i < n1; ++i)
    {
        sum1 += e1[i].budget;
        sum2 += e2[i].budget;
    }
    if (sum2 < sum1)
    {
        fprintf(stderr, "expected higher or equal total budget at deeper depth: %ld < %ld\n", sum2,
                sum1);
        rogue_dungeon_free_graph(&g);
        return 5;
    }

    rogue_dungeon_free_graph(&g);
    return 0;
}
