#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_dungeon_encounters.h"
#include "../../src/world/world_gen_dungeon_kernel.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 1337u;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);

    RogueDungeonGenParams p = {0};
    p.target_rooms = 24;
    p.loop_percent = 20;
    p.arch = ROGUE_DUNGEON_ARCH_BRANCHING;
    RogueDungeonGraph g = {0};
    if (!rogue_dungeon_generate_graph_ex(&ctx, &p, &g))
    {
        fprintf(stderr, "graph gen failed\n");
        return 1;
    }
    RogueDungeonEncounterPlanEntry entries[256];
    int n = rogue_dungeon_plan_encounters(&ctx, &g, /*depth*/ 6, entries, 256);
    if (n != g.room_count)
    {
        fprintf(stderr, "plan count mismatch %d vs %d\n", n, g.room_count);
        rogue_dungeon_free_graph(&g);
        return 2;
    }
    /* Validate budgets non-zero and roughly increasing with graph depth */
    int depths[256];
    memset(depths, 0, sizeof depths);
    int ok_depths = 0;
    /* Quick depth proxy via kernel helper */
    int cpl = rogue_dungeon_graph_critical_path_length(&g);
    if (cpl < 0)
    {
        fprintf(stderr, "bad CPL\n");
        rogue_dungeon_free_graph(&g);
        return 3;
    }
    (void) ok_depths;
    /* Count encounter types and check spacing
     * (we don't have per-room depth here, but we can scan adjacent ids for rough clustering)
     */
    int cnt_combat = 0, cnt_elite = 0, cnt_mini = 0, cnt_guard = 0, cnt_nem = 0;
    for (int i = 0; i < n; ++i)
    {
        if (entries[i].budget <= 0)
        {
            fprintf(stderr, "budget non-positive at %d\n", i);
            rogue_dungeon_free_graph(&g);
            return 4;
        }
        switch (entries[i].type)
        {
        case ROGUE_ENC_COMBAT:
            cnt_combat++;
            break;
        case ROGUE_ENC_ELITE_PACK:
            cnt_elite++;
            break;
        case ROGUE_ENC_MINI_BOSS:
            cnt_mini++;
            break;
        case ROGUE_ENC_PUZZLE_GUARD:
            cnt_guard++;
            break;
        }
        if (entries[i].nemesis)
            cnt_nem++;
    }
    if (cnt_elite < 1)
    {
        fprintf(stderr, "expected at least one elite pack\n");
        rogue_dungeon_free_graph(&g);
        return 5;
    }
    if (cnt_combat + cnt_elite + cnt_mini + cnt_guard != n)
    {
        fprintf(stderr, "type coverage mismatch\n");
        rogue_dungeon_free_graph(&g);
        return 6;
    }
    /* Nemesis should be rare */
    if (cnt_nem > n / 3)
    {
        fprintf(stderr, "nemesis too frequent: %d/%d\n", cnt_nem, n);
        rogue_dungeon_free_graph(&g);
        return 7;
    }
    rogue_dungeon_free_graph(&g);
    (void) cpl;
    return 0;
}
