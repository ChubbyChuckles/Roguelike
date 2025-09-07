#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_dungeon_kernel.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 1337u;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);

    RogueDungeonGraph g = {0};
    assert(rogue_dungeon_generate_graph(&ctx, 24, 20, &g) && "graph generation failed");

    int max_deg = 0;
    double avg_deg = 0.0;
    rogue_dungeon_graph_degree_stats(&g, &max_deg, &avg_deg);
    assert(max_deg >= 0);
    assert(avg_deg >= 0.0);

    int cpl = rogue_dungeon_graph_critical_path_length(&g);
    assert(cpl >= 0);

    unsigned long long h1 = rogue_dungeon_graph_hash(&g);
    rogue_dungeon_free_graph(&g);

    /* Determinism check: same seed should produce consistent hash */
    RogueDungeonGraph g2 = {0};
    rogue_worldgen_context_shutdown(&ctx);
    rogue_worldgen_context_init(&ctx, &cfg);
    assert(rogue_dungeon_generate_graph(&ctx, 24, 20, &g2));
    unsigned long long h2 = rogue_dungeon_graph_hash(&g2);
    assert(h1 == h2);
    rogue_dungeon_free_graph(&g2);

    rogue_worldgen_context_shutdown(&ctx);
    printf("ok\n");
    return 0;
}
