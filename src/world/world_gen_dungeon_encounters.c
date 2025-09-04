#include "world_gen_dungeon_encounters.h"
#include "world_gen_dungeon_kernel.h"
#include "world_gen_dungeon_taxonomy.h"
#include <stdlib.h>
#include <string.h>

/* Contract
 * inputs: ctx (seeded), graph (rooms, edges), depth
 * outputs: out[N<=max_entries] entries with one per room
 * success: returns count == graph->room_count
 * error modes: invalid args -> 0
 */

static int compute_room_depths(const RogueDungeonGraph* g, int* out_depths)
{
    if (!g || g->room_count <= 0)
        return 0;
    /* BFS from an arbitrary root (0) */
    int n = g->room_count;
    for (int i = 0; i < n; ++i)
        out_depths[i] = -1;
    int qcap = n;
    int* q = (int*) malloc((size_t) qcap * sizeof(int));
    if (!q)
        return 0;
    int qs = 0, qe = 0;
    q[qe++] = 0;
    out_depths[0] = 0;
    while (qs < qe)
    {
        int u = q[qs++];
        /* visit neighbors */
        for (int e = 0; e < g->edge_count; ++e)
        {
            const RogueDungeonEdge* ed = &g->edges[e];
            int v = -1;
            if (ed->a == u)
                v = ed->b;
            else if (ed->b == u)
                v = ed->a;
            else
                continue;
            if (out_depths[v] == -1)
            {
                out_depths[v] = out_depths[u] + 1;
                q[qe++] = v;
            }
        }
    }
    free(q);
    return 1;
}

int rogue_dungeon_plan_encounters(RogueWorldGenContext* ctx, const RogueDungeonGraph* graph,
                                  int depth, RogueDungeonEncounterPlanEntry* out, int max_entries)
{
    if (!ctx || !graph || !out || max_entries < graph->room_count)
        return 0;
    int n = graph->room_count;
    memset(out, 0, (size_t) max_entries * sizeof(*out));

    /* Determine target relative level delta and base budget per room */
    int dL = rogue_dungeon_target_level_delta(depth);
    int base_budget = 8 + dL * 2; /* grows with depth: 8,10,10,12,... */

    /* Compute room depths to approximate critical path by greatest depth */
    int* rdepths = (int*) malloc((size_t) n * sizeof(int));
    if (!rdepths)
        return 0;
    if (!compute_room_depths(graph, rdepths))
    {
        free(rdepths);
        return 0;
    }
    int maxd = 0;
    for (int i = 0; i < n; ++i)
        if (rdepths[i] > maxd)
            maxd = rdepths[i];

    /* Spacing: avoid clustering elites/miniboss within window K */
    const int windowK = 2;
    int last_elite_depth = -100;
    int last_miniboss_depth = -100;

    for (int i = 0; i < n; ++i)
    {
        RogueDungeonEncounterPlanEntry* pe = &out[i];
        pe->room_id = graph->rooms[i].id;
        pe->budget = base_budget;
        pe->type = ROGUE_ENC_COMBAT;
        pe->modifiers_mask = 0;
        pe->nemesis = 0;

        int rd = rdepths[i];
        /* Critical path approximation: deeper depth -> more budget */
        if (maxd > 0)
            pe->budget += (rd * 3) / (maxd > 0 ? maxd : 1);

        /* Tag influences: treasure rooms lean puzzle guard, elites get boosted */
        int tag = graph->rooms[i].tag;
        unsigned int rv = rogue_worldgen_rand_u32(&ctx->micro_rng);
        if (tag & ROGUE_DUNGEON_ROOM_TREASURE)
        {
            pe->type = ROGUE_ENC_PUZZLE_GUARD;
            pe->budget += 2;
        }
        else if ((rv % 100) < 15 && (rd - last_miniboss_depth) > windowK)
        {
            pe->type = ROGUE_ENC_MINI_BOSS;
            pe->budget += 6;
            last_miniboss_depth = rd;
        }
        else if ((rv % 100) < 40 && (rd - last_elite_depth) > windowK)
        {
            pe->type = ROGUE_ENC_ELITE_PACK;
            pe->budget += 3;
            last_elite_depth = rd;
        }
        else
        {
            pe->type = ROGUE_ENC_COMBAT;
        }

        /* Light nemesis injection hook: very rare, biased to deeper rooms */
        int nem_bp = 50 + rd * 10; /* 0.5% base + 0.1% per depth */
        if (nem_bp > 300)
            nem_bp = 300;
        pe->nemesis = ((int) (rv % 10000) < nem_bp) ? 1 : 0;
    }

    free(rdepths);
    return n;
}
