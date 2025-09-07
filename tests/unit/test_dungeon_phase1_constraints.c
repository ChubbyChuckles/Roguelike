#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_dungeon_kernel.h"
#include <assert.h>
#include <stdio.h>

static int count_deadends(const RogueDungeonGraph* g)
{
    int c = 0;
    for (int i = 0; i < g->room_count; ++i)
    {
        int deg = 0;
        for (int e = 0; e < g->edge_count; ++e)
        {
            const RogueDungeonEdge* ed = &g->edges[e];
            if (ed->a == i || ed->b == i)
                deg++;
        }
        if (deg == 1)
            c++;
    }
    return c;
}

int main(void)
{
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 424242u;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);

    RogueDungeonGenParams p = {0};
    p.target_rooms = 20;
    p.loop_percent = 20;
    p.max_deadends = 6;      /* cap leaf nodes */
    p.min_branch_degree = 2; /* avoid too skinny graph */
    p.max_branch_degree = 5; /* prevent hub overload */
    p.critical_path_target_min = 6;
    p.arch = ROGUE_DUNGEON_ARCH_BRANCHING;

    RogueDungeonGraph g = {0};
    assert(rogue_dungeon_generate_graph_ex(&ctx, &p, &g));
    assert(g.room_count > 0);
    assert(g.edge_count > 0);

    int cpl = rogue_dungeon_graph_critical_path_length(&g);
    assert(cpl >= 0);
    assert(cpl >= p.critical_path_target_min || g.room_count < 3);

    int leaves = count_deadends(&g);
    assert(p.max_deadends == 0 || leaves <= p.max_deadends);

    int max_deg = 0;
    double avg_deg = 0.0;
    rogue_dungeon_graph_degree_stats(&g, &max_deg, &avg_deg);
    assert(max_deg <= p.max_branch_degree || p.max_branch_degree == 0);

    unsigned long long h1 = rogue_dungeon_graph_hash(&g);
    rogue_dungeon_free_graph(&g);

    /* Determinism */
    RogueDungeonGraph g2 = {0};
    rogue_worldgen_context_shutdown(&ctx);
    rogue_worldgen_context_init(&ctx, &cfg);
    assert(rogue_dungeon_generate_graph_ex(&ctx, &p, &g2));
    unsigned long long h2 = rogue_dungeon_graph_hash(&g2);
    assert(h1 == h2);
    rogue_dungeon_free_graph(&g2);

    rogue_worldgen_context_shutdown(&ctx);
    printf("ok\n");
    return 0;
}
